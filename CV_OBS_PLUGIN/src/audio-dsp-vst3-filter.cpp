#include "CV_OBS_PLUGIN/AudioDspVst3Filter.hpp"
#include "CV_OBS_PLUGIN/InternalGainProcessor.hpp"
#include "CV_OBS_PLUGIN/ObsAudioBufferAdapter.hpp"
#include "CV_OBS_PLUGIN/Vst3StorageModel.hpp"

#include "backends/vst3sdk/public.sdk/vst/moduleinfo/json.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace cv_obs_plugin {
namespace {
constexpr const char *kBypassSetting = "bypass";
constexpr const char *kBypassLabel = "Bypass";
constexpr const char *kGainDbSetting = "internal_gain_db";
constexpr const char *kGainDbLabel = "Internal Gain (dB)";
constexpr const char *kVst3PluginSelectionSetting = "vst3_plugin_selection";
constexpr const char *kVst3PluginSelectionLabel = "VST3 Plugin";
constexpr const char *kNoPluginSelectionLabel = "No VST3 plugin selected";
constexpr double kMinGainDb = -24.0;
constexpr double kMaxGainDb = 24.0;
constexpr double kGainDbStep = 0.1;
constexpr double kDefaultGainDb = 0.0;
constexpr Steinberg::int32 kDefaultMaxSamplesPerBlock = 4096;
constexpr Steinberg::Vst::SampleRate kFallbackSampleRate = 48000.0;
constexpr Steinberg::int32 kFallbackChannelCount = 2;

[[nodiscard]] bool tuidEquals(const Steinberg::TUID left,
                              const Steinberg::TUID right) noexcept {
  return std::memcmp(left, right, sizeof(Steinberg::TUID)) == 0;
}

class MinimalVst3HostContext final : public Steinberg::Vst::IHostApplication {
public:
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                               void **obj) override {
    if (!obj)
      return Steinberg::kInvalidArgument;

    *obj = nullptr;
    if (tuidEquals(iid, INLINE_UID_OF(Steinberg::FUnknown)) ||
        tuidEquals(iid, INLINE_UID_OF(Steinberg::Vst::IHostApplication))) {
      *obj = static_cast<Steinberg::Vst::IHostApplication *>(this);
      addRef();
      return Steinberg::kResultOk;
    }

    return Steinberg::kNoInterface;
  }

  Steinberg::uint32 PLUGIN_API addRef() override {
    return refCount_.fetch_add(1, std::memory_order_relaxed) + 1;
  }

  Steinberg::uint32 PLUGIN_API release() override {
    const Steinberg::uint32 previous =
        refCount_.fetch_sub(1, std::memory_order_relaxed);
    return previous > 0 ? previous - 1 : 0;
  }

  Steinberg::tresult PLUGIN_API
  getName(Steinberg::Vst::String128 name) override {
    if (!name)
      return Steinberg::kInvalidArgument;

    std::fill(name, name + 128, 0);
    constexpr char16_t kHostName[] = u"AUDIO_DSP VST3";
    constexpr std::size_t kHostNameLength =
        sizeof(kHostName) / sizeof(kHostName[0]) - 1;
    std::copy_n(kHostName, std::min<std::size_t>(kHostNameLength, 127), name);
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API createInstance(Steinberg::TUID, Steinberg::TUID,
                                               void **obj) override {
    if (obj)
      *obj = nullptr;
    return Steinberg::kNoInterface;
  }

private:
  std::atomic<Steinberg::uint32> refCount_{1};
};

class SharedLibrary {
public:
#if defined(_WIN32)
  using Handle = HMODULE;
#else
  using Handle = void *;
#endif

  SharedLibrary() = default;
  explicit SharedLibrary(const std::filesystem::path &path) {
    static_cast<void>(open(path));
  }
  SharedLibrary(const SharedLibrary &) = delete;
  SharedLibrary &operator=(const SharedLibrary &) = delete;

  SharedLibrary(SharedLibrary &&other) noexcept { *this = std::move(other); }

  SharedLibrary &operator=(SharedLibrary &&other) noexcept {
    if (this == &other)
      return *this;

    close();
    handle_ = other.handle_;
    moduleExit_ = other.moduleExit_;
    other.handle_ = nullptr;
    other.moduleExit_ = nullptr;
    return *this;
  }

  ~SharedLibrary() { close(); }

  [[nodiscard]] bool open(const std::filesystem::path &path) {
    close();
#if defined(_WIN32)
    handle_ = LoadLibraryW(path.wstring().c_str());
    if (!handle_)
      return false;

    if (auto *initDll = reinterpret_cast<bool(PLUGIN_API *)()>(
            GetProcAddress(handle_, "InitDll"))) {
      if (!initDll()) {
        closeWithoutModuleExit();
        return false;
      }
      moduleExit_ = reinterpret_cast<bool(PLUGIN_API *)()>(
          GetProcAddress(handle_, "ExitDll"));
    }
#else
    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_)
      return false;

    if (auto *moduleEntry = reinterpret_cast<bool(PLUGIN_API *)(void *)>(
            dlsym(handle_, "ModuleEntry"))) {
      if (!moduleEntry(handle_)) {
        closeWithoutModuleExit();
        return false;
      }
      moduleExit_ =
          reinterpret_cast<bool(PLUGIN_API *)()>(dlsym(handle_, "ModuleExit"));
    }
#endif
    return true;
  }

  void close() noexcept {
    if (!handle_)
      return;

    if (moduleExit_)
      moduleExit_();
    closeWithoutModuleExit();
  }

  [[nodiscard]] bool isOpen() const noexcept { return handle_ != nullptr; }

  template <typename Proc>
  [[nodiscard]] Proc loadSymbol(const char *name) const {
    if (!handle_)
      return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<Proc>(GetProcAddress(handle_, name));
#else
    return reinterpret_cast<Proc>(dlsym(handle_, name));
#endif
  }

private:
  void closeWithoutModuleExit() noexcept {
#if defined(_WIN32)
    if (handle_)
      FreeLibrary(handle_);
#else
    if (handle_)
      dlclose(handle_);
#endif
    handle_ = nullptr;
    moduleExit_ = nullptr;
  }

  Handle handle_ = nullptr;
  bool(PLUGIN_API *moduleExit_)() = nullptr;
};

struct Vst3PluginInstance {
  ~Vst3PluginInstance() { reset(); }

  void reset() noexcept {
    if (audioProcessor)
      audioProcessor->setProcessing(false);

    if (component)
      component->setActive(false);

    if (component)
      component->terminate();

    if (audioProcessor) {
      audioProcessor->release();
      audioProcessor = nullptr;
    }

    if (component) {
      component->release();
      component = nullptr;
    }

    factory = nullptr;
    library.reset();
    componentClassId.clear();
    pluginPath.clear();
    channelCount = 0;
    sampleRate = 0.0;
    maxSamplesPerBlock = 0;
  }

  std::unique_ptr<SharedLibrary> library;
  Steinberg::IPluginFactory *factory = nullptr;
  Steinberg::Vst::IComponent *component = nullptr;
  Steinberg::Vst::IAudioProcessor *audioProcessor = nullptr;
  std::string pluginPath;
  std::string componentClassId;
  Steinberg::int32 channelCount = 0;
  Steinberg::Vst::SampleRate sampleRate = 0.0;
  Steinberg::int32 maxSamplesPerBlock = 0;
};

struct AudioDspVst3FilterState {
  obs_source_t *source = nullptr;
  std::atomic_bool bypass{true};
  std::atomic<float> linearGain{1.0F};
  std::atomic_bool backendReloadRequested{false};
  std::mutex selectedPluginMutex;
  std::mutex vst3InstanceMutex;
  std::string selectedPluginIdentifier;
  std::unique_ptr<MinimalVst3HostContext> hostContext{
      std::make_unique<MinimalVst3HostContext>()};
  Vst3PluginInstance vst3Instance;
  InternalGainProcessor gainProcessor;
};

const char *getName(void *) { return kPluginDisplayName; }

[[nodiscard]] std::string makeModuleConfigRelativePath(const char *fileName) {
  std::string relativePath = vst3_storage::kRootDirectoryName;
  relativePath += '/';
  relativePath += fileName;
  return relativePath;
}

[[nodiscard]] bool pathExists(const std::filesystem::path &path) noexcept {
  std::error_code error;
  return std::filesystem::exists(path, error) && !error;
}

[[nodiscard]] std::filesystem::path resolveCachePath() {
  const std::string relativeCachePath =
      makeModuleConfigRelativePath(vst3_storage::kCacheFileName);

  if (char *configPath = obs_module_config_path(relativeCachePath.c_str())) {
    std::filesystem::path cachePath(configPath);
    bfree(configPath);
    if (pathExists(cachePath))
      return cachePath;
  }

  std::error_code error;
  const std::filesystem::path currentPath =
      std::filesystem::current_path(error);
  if (error)
    return {};

  const std::filesystem::path localObsCachePath =
      currentPath / vst3_storage::kRootDirectoryName /
      vst3_storage::kCacheFileName;
  if (pathExists(localObsCachePath))
    return localObsCachePath;

  const std::filesystem::path localCachePath =
      currentPath / vst3_storage::kCacheFileName;
  if (pathExists(localCachePath))
    return localCachePath;

  return {};
}

[[nodiscard]] json_value_s *findObjectValue(json_object_s *object,
                                            std::string_view key) noexcept {
  if (!object)
    return nullptr;

  for (json_object_element_s *element = object->start; element;
       element = element->next) {
    if (!element->name || !element->name->string)
      continue;

    if (key.size() == element->name->string_size &&
        std::string_view(element->name->string, element->name->string_size) ==
            key)
      return element->value;
  }

  return nullptr;
}

[[nodiscard]] json_string_s *valueAsString(json_value_s *value) noexcept {
  if (!value || value->type != json_type_string)
    return nullptr;
  return json_value_as_string(value);
}

[[nodiscard]] json_number_s *valueAsNumber(json_value_s *value) noexcept {
  if (!value || value->type != json_type_number)
    return nullptr;
  return json_value_as_number(value);
}

[[nodiscard]] json_object_s *valueAsObject(json_value_s *value) noexcept {
  if (!value || value->type != json_type_object)
    return nullptr;
  return json_value_as_object(value);
}

[[nodiscard]] json_array_s *valueAsArray(json_value_s *value) noexcept {
  if (!value || value->type != json_type_array)
    return nullptr;
  return json_value_as_array(value);
}

[[nodiscard]] std::string readJsonString(json_object_s *object,
                                         std::string_view key) {
  json_string_s *stringValue = valueAsString(findObjectValue(object, key));
  if (!stringValue || !stringValue->string)
    return {};

  return std::string(stringValue->string, stringValue->string_size);
}

[[nodiscard]] std::uint64_t readJsonUint64(json_object_s *object,
                                           std::string_view key) noexcept {
  json_number_s *numberValue = valueAsNumber(findObjectValue(object, key));
  if (!numberValue || !numberValue->number)
    return 0;

  std::uint64_t result = 0;
  const char *begin = numberValue->number;
  const char *end = begin + numberValue->number_size;
  const std::from_chars_result conversion = std::from_chars(begin, end, result);
  if (conversion.ec != std::errc() || conversion.ptr != end)
    return 0;

  return result;
}

[[nodiscard]] std::uint32_t readJsonUint32(json_object_s *object,
                                           std::string_view key) {
  return static_cast<std::uint32_t>(readJsonUint64(object, key));
}

[[nodiscard]] Vst3ScanStatus
scanStatusFromString(std::string_view status) noexcept {
  if (status == "CandidateFound")
    return Vst3ScanStatus::CandidateFound;
  if (status == "MetadataRead")
    return Vst3ScanStatus::MetadataRead;
  if (status == "Failed")
    return Vst3ScanStatus::Failed;
  if (status == "Blacklisted")
    return Vst3ScanStatus::Blacklisted;
  return Vst3ScanStatus::Unknown;
}

[[nodiscard]] Vst3ScanStatus
scanStatusFromNumber(std::uint64_t status) noexcept {
  switch (status) {
  case 1:
    return Vst3ScanStatus::CandidateFound;
  case 2:
    return Vst3ScanStatus::MetadataRead;
  case 3:
    return Vst3ScanStatus::Failed;
  case 4:
    return Vst3ScanStatus::Blacklisted;
  default:
    return Vst3ScanStatus::Unknown;
  }
}

[[nodiscard]] Vst3ScanStatus readJsonScanStatus(json_object_s *object,
                                                std::string_view key) {
  json_value_s *statusValue = findObjectValue(object, key);
  if (valueAsString(statusValue))
    return scanStatusFromString(readJsonString(object, key));
  if (valueAsNumber(statusValue))
    return scanStatusFromNumber(readJsonUint64(object, key));
  return Vst3ScanStatus::Unknown;
}

[[nodiscard]] Vst3PluginCacheEntry readCacheEntry(json_object_s *pluginObject) {
  Vst3PluginCacheEntry entry;
  entry.path = readJsonString(pluginObject, "path");
  entry.name = readJsonString(pluginObject, "name");
  entry.vendor = readJsonString(pluginObject, "vendor");
  entry.version = readJsonString(pluginObject, "version");
  entry.classId = readJsonString(pluginObject, "classId");
  entry.category = readJsonString(pluginObject, "category");
  entry.status = readJsonScanStatus(pluginObject, "status");
  entry.lastModifiedUnixSeconds =
      readJsonUint64(pluginObject, "lastModifiedUnixSeconds");
  entry.scannedAtUnixSeconds =
      readJsonUint64(pluginObject, "scannedAtUnixSeconds");
  return entry;
}

[[nodiscard]] Vst3PluginCacheDocument
loadVst3PluginCache(const std::filesystem::path &cachePath) {
  Vst3PluginCacheDocument document;
  if (cachePath.empty())
    return document;

  std::ifstream cacheFile(cachePath, std::ios::binary);
  if (!cacheFile)
    return document;

  std::string jsonText{std::istreambuf_iterator<char>(cacheFile),
                       std::istreambuf_iterator<char>()};
  if (jsonText.empty())
    return document;

  json_value_s *rootValue = json_parse(jsonText.data(), jsonText.size());
  if (!rootValue) {
    blog(LOG_WARNING, "AUDIO_DSP VST3: failed to parse VST3 cache file: %s",
         cachePath.string().c_str());
    return document;
  }

  std::unique_ptr<json_value_s, decltype(&std::free)> parsedJson(rootValue,
                                                                 &std::free);
  json_object_s *rootObject = valueAsObject(parsedJson.get());
  if (!rootObject)
    return document;

  const std::uint32_t schemaVersion =
      readJsonUint32(rootObject, "schemaVersion");
  if (schemaVersion > 0)
    document.schemaVersion = schemaVersion;

  json_array_s *plugins = valueAsArray(findObjectValue(rootObject, "plugins"));
  if (!plugins)
    return document;

  document.plugins.reserve(plugins->length);
  for (json_array_element_s *element = plugins->start; element;
       element = element->next) {
    json_object_s *pluginObject = valueAsObject(element->value);
    if (!pluginObject)
      continue;

    Vst3PluginCacheEntry entry = readCacheEntry(pluginObject);
    const bool hasUsableMetadata = !entry.path.empty() && !entry.name.empty();
    const bool isRejected = entry.status == Vst3ScanStatus::Failed ||
                            entry.status == Vst3ScanStatus::Blacklisted;
    if (hasUsableMetadata && !isRejected)
      document.plugins.push_back(std::move(entry));
  }

  return document;
}

[[nodiscard]] std::string
makePluginDisplayName(const Vst3PluginCacheEntry &entry) {
  std::ostringstream displayName;
  displayName << entry.name;
  if (!entry.vendor.empty())
    displayName << " — " << entry.vendor;
  return displayName.str();
}

void populateVst3PluginList(obs_property_t *pluginList) {
  if (!pluginList)
    return;

  obs_property_list_add_string(pluginList, kNoPluginSelectionLabel, "");

  const std::filesystem::path cachePath = resolveCachePath();
  const Vst3PluginCacheDocument cacheDocument = loadVst3PluginCache(cachePath);
  for (const Vst3PluginCacheEntry &entry : cacheDocument.plugins) {
    const std::string displayName = makePluginDisplayName(entry);
    const std::string &storedIdentifier =
        entry.classId.empty() ? entry.path : entry.classId;
    obs_property_list_add_string(pluginList, displayName.c_str(),
                                 storedIdentifier.c_str());
  }
}

[[nodiscard]] std::optional<Vst3PluginCacheEntry>
findCachedPluginEntry(std::string_view selectedIdentifier) {
  if (selectedIdentifier.empty())
    return std::nullopt;

  const Vst3PluginCacheDocument cacheDocument =
      loadVst3PluginCache(resolveCachePath());
  for (const Vst3PluginCacheEntry &entry : cacheDocument.plugins) {
    if (entry.path == selectedIdentifier || entry.classId == selectedIdentifier)
      return entry;
  }

  std::filesystem::path selectedPath{std::string(selectedIdentifier)};
  if (pathExists(selectedPath)) {
    Vst3PluginCacheEntry fallbackEntry;
    fallbackEntry.path = selectedPath.string();
    return fallbackEntry;
  }

  return std::nullopt;
}

[[nodiscard]] int hexValue(char character) noexcept {
  if (character >= '0' && character <= '9')
    return character - '0';
  if (character >= 'a' && character <= 'f')
    return character - 'a' + 10;
  if (character >= 'A' && character <= 'F')
    return character - 'A' + 10;
  return -1;
}

[[nodiscard]] bool parseVst3ClassId(std::string_view classId,
                                    Steinberg::TUID output) noexcept {
  std::array<char, 32> hexDigits{};
  std::size_t hexDigitCount = 0;

  for (const char character : classId) {
    if (!std::isxdigit(static_cast<unsigned char>(character)))
      continue;

    if (hexDigitCount >= hexDigits.size())
      return false;

    hexDigits[hexDigitCount++] = character;
  }

  if (hexDigitCount != hexDigits.size())
    return false;

  for (std::size_t byte = 0; byte < sizeof(Steinberg::TUID); ++byte) {
    const int high = hexValue(hexDigits[byte * 2]);
    const int low = hexValue(hexDigits[byte * 2 + 1]);
    if (high < 0 || low < 0)
      return false;

    output[byte] = static_cast<char>((high << 4) | low);
  }

  return true;
}

[[nodiscard]] std::filesystem::path
resolveVst3BinaryPath(const std::filesystem::path &pluginPath) {
  if (pluginPath.empty())
    return {};

  std::error_code error;
  if (std::filesystem::is_regular_file(pluginPath, error) && !error)
    return pluginPath;

  error.clear();
  if (!std::filesystem::is_directory(pluginPath, error) || error)
    return {};

  const std::string stem = pluginPath.stem().string();
  const std::array<std::filesystem::path, 5> candidates = {
#if defined(_WIN32)
      pluginPath / "Contents" / "x86_64-win" / (stem + ".dll"),
      pluginPath / "Contents" / "x86-win" / (stem + ".dll"),
      pluginPath / (stem + ".dll"),
      pluginPath / "Contents" / "x86_64-linux" / (stem + ".so"),
      pluginPath / "Contents" / "MacOS" / stem,
#elif defined(__APPLE__)
      pluginPath / "Contents" / "MacOS" / stem,
      pluginPath / "Contents" / "MacOS" / (stem + ".dylib"),
      pluginPath / "Contents" / "x86_64-linux" / (stem + ".so"),
      pluginPath / "Contents" / "x86_64-win" / (stem + ".dll"),
      pluginPath / stem,
#else
      pluginPath / "Contents" / "x86_64-linux" / (stem + ".so"),
      pluginPath / "Contents" / "amd64-linux" / (stem + ".so"),
      pluginPath / (stem + ".so"),
      pluginPath / "Contents" / "MacOS" / stem,
      pluginPath / "Contents" / "x86_64-win" / (stem + ".dll"),
#endif
  };

  for (const std::filesystem::path &candidate : candidates) {
    if (pathExists(candidate))
      return candidate;
  }

  return {};
}

[[nodiscard]] Steinberg::Vst::SpeakerArrangement
speakerArrangementForChannelCount(Steinberg::int32 channelCount) noexcept {
  using namespace Steinberg::Vst;

  switch (channelCount) {
  case 1:
    return SpeakerArr::kMono;
  case 2:
    return SpeakerArr::kStereo;
  case 3:
    return SpeakerArr::k30Cine;
  case 4:
    return SpeakerArr::k40Cine;
  case 5:
    return SpeakerArr::k50;
  case 6:
    return SpeakerArr::k51;
  case 7:
    return SpeakerArr::k70Cine;
  case 8:
    return SpeakerArr::k71Cine;
  default:
    return SpeakerArr::kStereo;
  }
}

struct ObsAudioConfiguration {
  Steinberg::Vst::SampleRate sampleRate = kFallbackSampleRate;
  Steinberg::int32 channelCount = kFallbackChannelCount;
  Steinberg::int32 maxSamplesPerBlock = kDefaultMaxSamplesPerBlock;
};

[[nodiscard]] ObsAudioConfiguration readObsAudioConfiguration() noexcept {
  ObsAudioConfiguration configuration;

  obs_audio_info audioInfo = {};
  if (obs_get_audio_info(&audioInfo)) {
    if (audioInfo.samples_per_sec > 0)
      configuration.sampleRate =
          static_cast<Steinberg::Vst::SampleRate>(audioInfo.samples_per_sec);

    const std::size_t channels = get_audio_channels(audioInfo.speakers);
    if (channels > 0)
      configuration.channelCount = static_cast<Steinberg::int32>(
          std::min<std::size_t>(channels, MAX_AV_PLANES));
  }

  return configuration;
}

[[nodiscard]] bool busExists(Steinberg::Vst::IComponent &component,
                             Steinberg::Vst::BusDirection direction,
                             Steinberg::int32 index) {
  Steinberg::Vst::BusInfo busInfo = {};
  return component.getBusInfo(Steinberg::Vst::kAudio, direction, index,
                              busInfo) == Steinberg::kResultOk;
}

void logVst3LoadError(const char *message, const std::string &pluginPath) {
  blog(LOG_ERROR, "AUDIO_DSP VST3: %s: %s", message, pluginPath.c_str());
}

[[nodiscard]] bool
initializeSelectedVst3PluginLocked(AudioDspVst3FilterState &state,
                                   std::string_view selectedIdentifier) {
  state.vst3Instance.reset();

  const std::optional<Vst3PluginCacheEntry> selectedEntry =
      findCachedPluginEntry(selectedIdentifier);
  if (!selectedEntry) {
    blog(LOG_ERROR,
         "AUDIO_DSP VST3: selected plugin was not found in VST3 cache: %.*s",
         static_cast<int>(selectedIdentifier.size()),
         selectedIdentifier.data());
    return false;
  }

  if (selectedEntry->path.empty()) {
    blog(LOG_ERROR,
         "AUDIO_DSP VST3: selected VST3 cache entry does not contain a path: "
         "%.*s",
         static_cast<int>(selectedIdentifier.size()),
         selectedIdentifier.data());
    return false;
  }

  if (selectedEntry->classId.empty()) {
    logVst3LoadError("selected VST3 cache entry does not contain a class ID",
                     selectedEntry->path);
    return false;
  }

  Steinberg::TUID componentClassId = {};
  if (!parseVst3ClassId(selectedEntry->classId, componentClassId)) {
    logVst3LoadError("selected VST3 cache entry contains an invalid class ID",
                     selectedEntry->path);
    return false;
  }

  const std::filesystem::path binaryPath =
      resolveVst3BinaryPath(selectedEntry->path);
  if (binaryPath.empty()) {
    logVst3LoadError("could not resolve VST3 binary path", selectedEntry->path);
    return false;
  }

  state.vst3Instance.library = std::make_unique<SharedLibrary>(binaryPath);
  if (!state.vst3Instance.library->isOpen()) {
    logVst3LoadError("failed to load VST3 dynamic library",
                     binaryPath.string());
    state.vst3Instance.reset();
    return false;
  }

  using GetPluginFactoryProc = Steinberg::IPluginFactory *(PLUGIN_API *)();
  GetPluginFactoryProc getPluginFactory =
      state.vst3Instance.library->loadSymbol<GetPluginFactoryProc>(
          "GetPluginFactory");
  if (!getPluginFactory) {
    logVst3LoadError("VST3 binary does not export GetPluginFactory",
                     binaryPath.string());
    state.vst3Instance.reset();
    return false;
  }

  state.vst3Instance.factory = getPluginFactory();
  if (!state.vst3Instance.factory) {
    logVst3LoadError("GetPluginFactory returned null", binaryPath.string());
    state.vst3Instance.reset();
    return false;
  }

  void *componentObject = nullptr;
  if (state.vst3Instance.factory->createInstance(
          componentClassId, INLINE_UID_OF(Steinberg::Vst::IComponent),
          &componentObject) != Steinberg::kResultOk ||
      !componentObject) {
    logVst3LoadError("failed to create VST3 component", selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  state.vst3Instance.component =
      static_cast<Steinberg::Vst::IComponent *>(componentObject);
  state.vst3Instance.component->setIoMode(Steinberg::Vst::kSimple);

  void *audioProcessorObject = nullptr;
  if (state.vst3Instance.component->queryInterface(
          INLINE_UID_OF(Steinberg::Vst::IAudioProcessor),
          &audioProcessorObject) != Steinberg::kResultOk ||
      !audioProcessorObject) {
    logVst3LoadError("VST3 component does not implement IAudioProcessor",
                     selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  state.vst3Instance.audioProcessor =
      static_cast<Steinberg::Vst::IAudioProcessor *>(audioProcessorObject);

  if (state.vst3Instance.component->initialize(state.hostContext.get()) !=
      Steinberg::kResultOk) {
    logVst3LoadError("VST3 component initialization failed",
                     selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  const Steinberg::int32 inputBusCount =
      state.vst3Instance.component->getBusCount(Steinberg::Vst::kAudio,
                                                Steinberg::Vst::kInput);
  const Steinberg::int32 outputBusCount =
      state.vst3Instance.component->getBusCount(Steinberg::Vst::kAudio,
                                                Steinberg::Vst::kOutput);
  if (inputBusCount <= 0 || outputBusCount <= 0 ||
      !busExists(*state.vst3Instance.component, Steinberg::Vst::kInput, 0) ||
      !busExists(*state.vst3Instance.component, Steinberg::Vst::kOutput, 0)) {
    logVst3LoadError(
        "VST3 component does not expose main audio input/output buses",
        selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  const ObsAudioConfiguration audioConfiguration = readObsAudioConfiguration();
  const Steinberg::Vst::SpeakerArrangement arrangement =
      speakerArrangementForChannelCount(audioConfiguration.channelCount);

  std::vector<Steinberg::Vst::SpeakerArrangement> inputArrangements(
      static_cast<std::size_t>(inputBusCount),
      Steinberg::Vst::SpeakerArr::kEmpty);
  std::vector<Steinberg::Vst::SpeakerArrangement> outputArrangements(
      static_cast<std::size_t>(outputBusCount),
      Steinberg::Vst::SpeakerArr::kEmpty);
  inputArrangements.front() = arrangement;
  outputArrangements.front() = arrangement;

  if (state.vst3Instance.audioProcessor->setBusArrangements(
          inputArrangements.data(), inputBusCount, outputArrangements.data(),
          outputBusCount) != Steinberg::kResultOk) {
    logVst3LoadError("VST3 bus arrangement setup failed", selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  if (state.vst3Instance.audioProcessor->canProcessSampleSize(
          Steinberg::Vst::kSample32) != Steinberg::kResultOk) {
    logVst3LoadError("VST3 component does not support FP32 processing",
                     selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  Steinberg::Vst::ProcessSetup processSetup = {};
  processSetup.processMode = Steinberg::Vst::kRealtime;
  processSetup.symbolicSampleSize = Steinberg::Vst::kSample32;
  processSetup.maxSamplesPerBlock = audioConfiguration.maxSamplesPerBlock;
  processSetup.sampleRate = audioConfiguration.sampleRate;

  if (state.vst3Instance.audioProcessor->setupProcessing(processSetup) !=
      Steinberg::kResultOk) {
    logVst3LoadError("VST3 process setup failed", selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  if (state.vst3Instance.component->activateBus(Steinberg::Vst::kAudio,
                                                Steinberg::Vst::kInput, 0,
                                                true) != Steinberg::kResultOk ||
      state.vst3Instance.component->activateBus(Steinberg::Vst::kAudio,
                                                Steinberg::Vst::kOutput, 0,
                                                true) != Steinberg::kResultOk) {
    logVst3LoadError("VST3 main audio bus activation failed",
                     selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  if (state.vst3Instance.component->setActive(true) != Steinberg::kResultOk) {
    logVst3LoadError("VST3 component activation failed", selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  if (state.vst3Instance.audioProcessor->setProcessing(true) !=
      Steinberg::kResultOk) {
    logVst3LoadError("VST3 processing activation failed", selectedEntry->path);
    state.vst3Instance.reset();
    return false;
  }

  state.vst3Instance.pluginPath = selectedEntry->path;
  state.vst3Instance.componentClassId = selectedEntry->classId;
  state.vst3Instance.channelCount = audioConfiguration.channelCount;
  state.vst3Instance.sampleRate = audioConfiguration.sampleRate;
  state.vst3Instance.maxSamplesPerBlock = audioConfiguration.maxSamplesPerBlock;

  blog(LOG_INFO, "AUDIO_DSP VST3: loaded VST3 plugin: %s",
       selectedEntry->path.c_str());
  return true;
}

void reloadSelectedVst3Plugin(AudioDspVst3FilterState &state,
                              std::string_view selectedIdentifier) {
  std::lock_guard<std::mutex> lock(state.vst3InstanceMutex);
  if (selectedIdentifier.empty()) {
    state.vst3Instance.reset();
    state.backendReloadRequested.store(false, std::memory_order_release);
    return;
  }

  static_cast<void>(
      initializeSelectedVst3PluginLocked(state, selectedIdentifier));
  state.backendReloadRequested.store(false, std::memory_order_release);
}

float gainDbToLinear(double gainDb) noexcept {
  const double clampedGainDb = std::clamp(gainDb, kMinGainDb, kMaxGainDb);
  return static_cast<float>(std::pow(10.0, clampedGainDb / 20.0));
}

void update(void *data, obs_data_t *settings) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  if (!state || !settings)
    return;

  state->bypass.store(obs_data_get_bool(settings, kBypassSetting),
                      std::memory_order_relaxed);
  state->linearGain.store(
      gainDbToLinear(obs_data_get_double(settings, kGainDbSetting)),
      std::memory_order_relaxed);

  const char *selectedPlugin =
      obs_data_get_string(settings, kVst3PluginSelectionSetting);
  const std::string selectedPluginIdentifier =
      selectedPlugin ? selectedPlugin : "";

  bool selectedPluginChanged = false;
  {
    std::lock_guard<std::mutex> lock(state->selectedPluginMutex);
    if (state->selectedPluginIdentifier != selectedPluginIdentifier) {
      state->selectedPluginIdentifier = selectedPluginIdentifier;
      state->backendReloadRequested.store(true, std::memory_order_release);
      selectedPluginChanged = true;
    }
  }

  if (selectedPluginChanged)
    reloadSelectedVst3Plugin(*state, selectedPluginIdentifier);
}

void *create(obs_data_t *settings, obs_source_t *source) {
  auto *state = new AudioDspVst3FilterState;
  state->source = source;
  update(state, settings);
  return state;
}

void destroy(void *data) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  delete state;
}

void getDefaults(obs_data_t *settings) {
  obs_data_set_default_bool(settings, kBypassSetting, true);
  obs_data_set_default_double(settings, kGainDbSetting, kDefaultGainDb);
  obs_data_set_default_string(settings, kVst3PluginSelectionSetting, "");
}

obs_properties_t *getProperties(void *) {
  obs_properties_t *properties = obs_properties_create();
  obs_properties_add_bool(properties, kBypassSetting, kBypassLabel);
  obs_property_t *pluginList = obs_properties_add_list(
      properties, kVst3PluginSelectionSetting, kVst3PluginSelectionLabel,
      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  populateVst3PluginList(pluginList);

  obs_properties_add_float_slider(properties, kGainDbSetting, kGainDbLabel,
                                  kMinGainDb, kMaxGainDb, kGainDbStep);
  return properties;
}

obs_audio_data *filterAudio(void *data, obs_audio_data *audio) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  if (!state)
    return audio;

  if (!audio)
    return nullptr;

  ObsAudioBufferAdapter bufferAdapter(audio);
  if (!bufferAdapter.isValid())
    return audio;

  if (state->bypass.load(std::memory_order_relaxed))
    return audio;

  auto audioView = bufferAdapter.audioView();
  state->gainProcessor.process(
      audioView, state->linearGain.load(std::memory_order_relaxed));
  return audio;
}
} // namespace

obs_source_info makeAudioDspVst3FilterInfo() noexcept {
  obs_source_info info = {};
  info.id = kPluginId;
  info.type = OBS_SOURCE_TYPE_FILTER;
  info.output_flags = OBS_SOURCE_AUDIO;
  info.get_name = getName;
  info.create = create;
  info.destroy = destroy;
  info.get_defaults = getDefaults;
  info.get_properties = getProperties;
  info.update = update;
  info.filter_audio = filterAudio;
  return info;
}

} // namespace cv_obs_plugin
