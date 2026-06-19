#include "CV_OBS_PLUGIN/AudioDspVst3Filter.hpp"
#include "CV_OBS_PLUGIN/InternalGainProcessor.hpp"
#include "CV_OBS_PLUGIN/ObsAudioBufferAdapter.hpp"
#include "CV_OBS_PLUGIN/Vst3StorageModel.hpp"
#include "CV_OBS_PLUGIN/Vst3ParameterInfo.hpp"
#include "CV_OBS_PLUGIN/Vst3Scanner.hpp"
#include "CV_OBS_PLUGIN/Vst3ImGuiFallback.hpp"

#include "backends/vst3sdk/public.sdk/vst/moduleinfo/json.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
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
constexpr const char *kOpenPluginEditorButton = "open_plugin_interface";
constexpr const char *kOpenPluginEditorLabel = "Open Plugin Interface";
constexpr const char *kScanPluginsButton = "scan_vst3_plugins";
constexpr const char *kScanPluginsLabel = "Scan VST3 Plugins";
constexpr const char *kVst3ParamKeyPrefix = "vst3p_";
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

class MinimalComponentHandler final : public Steinberg::Vst::IComponentHandler {
public:
  explicit MinimalComponentHandler(obs_source_t *source = nullptr)
      : source_(source) {}

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                               void **obj) override {
    if (!obj)
      return Steinberg::kInvalidArgument;

    *obj = nullptr;
    if (tuidEquals(iid, INLINE_UID_OF(Steinberg::FUnknown)) ||
        tuidEquals(iid, INLINE_UID_OF(Steinberg::Vst::IComponentHandler))) {
      *obj = static_cast<Steinberg::Vst::IComponentHandler *>(this);
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

  Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override {
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API
  performEdit(Steinberg::Vst::ParamID, Steinberg::Vst::ParamValue) override {
    // Plugin alterou parâmetro via GUI nativa — atualiza painel de propriedades do OBS
    if (source_)
      obs_source_update_properties(source_);
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID) override {
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32) override {
    return Steinberg::kResultOk;
  }

private:
  obs_source_t *source_ = nullptr;
  std::atomic<Steinberg::uint32> refCount_{1};
};

class MinimalPlugFrame final : public Steinberg::IPlugFrame {
public:
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                               void **obj) override {
    if (!obj)
      return Steinberg::kInvalidArgument;

    *obj = nullptr;
    if (tuidEquals(iid, INLINE_UID_OF(Steinberg::FUnknown)) ||
        tuidEquals(iid, INLINE_UID_OF(Steinberg::IPlugFrame))) {
      *obj = static_cast<Steinberg::IPlugFrame *>(this);
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
  resizeView(Steinberg::IPlugView *view, Steinberg::ViewRect *newSize) override;

#if defined(_WIN32)
  HWND hwnd = nullptr;
#elif defined(__linux__)
  Display *x11Display = nullptr;
  Window   x11Win     = 0;
#endif

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

struct Vst3EditorWindow {
  void close() noexcept {
    if (plugView) {
      if (attached)
        plugView->removed();
      plugView->release();
      plugView = nullptr;
    }
    attached = false;

#if defined(_WIN32)
    if (hwnd) {
      HWND windowToDestroy = hwnd;
      hwnd = nullptr;
      DestroyWindow(windowToDestroy);
    }
    plugFrame.hwnd = nullptr;
#elif defined(__linux__)
    if (x11EventThread.joinable()) {
      x11EventThreadRunning.store(false, std::memory_order_release);
      x11EventThread.join();
    }
    if (x11Display && x11Win) {
      XDestroyWindow(x11Display, x11Win);
      XCloseDisplay(x11Display);
      x11Win     = 0;
      x11Display = nullptr;
    }
    plugFrame.x11Display = nullptr;
    plugFrame.x11Win     = 0;
#endif
  }

  Steinberg::IPlugView *plugView = nullptr;
  bool attached = false;
  MinimalPlugFrame plugFrame;

#if defined(_WIN32)
  HWND hwnd = nullptr;
#elif defined(__linux__)
  std::atomic_bool x11EventThreadRunning{false};
  std::thread      x11EventThread;
  Display         *x11Display = nullptr;
  Window           x11Win     = 0;
#endif
};

#if defined(_WIN32)
constexpr const wchar_t *kEditorWindowClassName = L"AUDIO_DSP_VST3_Editor";

LRESULT CALLBACK editorWindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                  LPARAM lParam) {
  auto *editorWindow = reinterpret_cast<Vst3EditorWindow *>(
      GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (message) {
  case WM_NCCREATE: {
    const auto *createStruct = reinterpret_cast<CREATESTRUCTW *>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    return TRUE;
  }
  case WM_CLOSE:
    if (editorWindow)
      editorWindow->close();
    return 0;
  case WM_NCDESTROY:
    if (editorWindow) {
      if (editorWindow->plugView && editorWindow->attached)
        editorWindow->plugView->removed();
      if (editorWindow->plugView)
        editorWindow->plugView->release();
      editorWindow->plugView = nullptr;
      editorWindow->attached = false;
      editorWindow->hwnd = nullptr;
      editorWindow->plugFrame.hwnd = nullptr;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    }
    break;
  default:
    break;
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

[[nodiscard]] bool ensureEditorWindowClassRegistered() noexcept {
  static std::atomic_bool registered{false};
  if (registered.load(std::memory_order_acquire))
    return true;

  WNDCLASSEXW windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = editorWindowProc;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  windowClass.lpszClassName = kEditorWindowClassName;

  if (!RegisterClassExW(&windowClass) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return false;

  registered.store(true, std::memory_order_release);
  return true;
}

[[nodiscard]] int viewRectWidth(const Steinberg::ViewRect &rect) noexcept {
  return static_cast<int>(
      std::max<Steinberg::CCoord>(rect.right - rect.left, 1));
}

[[nodiscard]] int viewRectHeight(const Steinberg::ViewRect &rect) noexcept {
  return static_cast<int>(
      std::max<Steinberg::CCoord>(rect.bottom - rect.top, 1));
}

Steinberg::tresult MinimalPlugFrame::resizeView(Steinberg::IPlugView *view,
                                                Steinberg::ViewRect *newSize) {
  if (!hwnd || !view || !newSize)
    return Steinberg::kInvalidArgument;

  RECT windowRect = {0, 0, viewRectWidth(*newSize), viewRectHeight(*newSize)};
  AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0);
  SetWindowPos(hwnd, nullptr, 0, 0, windowRect.right - windowRect.left,
               windowRect.bottom - windowRect.top, SWP_NOMOVE | SWP_NOZORDER);
  return view->onSize(newSize);
}
#elif defined(__linux__)
Steinberg::tresult MinimalPlugFrame::resizeView(Steinberg::IPlugView *view,
                                                Steinberg::ViewRect *newSize) {
  if (!view || !newSize)
    return Steinberg::kInvalidArgument;
  // Redimensiona a janela X11 para acomodar o novo tamanho solicitado pelo plugin
  if (x11Display && x11Win) {
    const auto w = static_cast<unsigned>(
        std::max<Steinberg::CCoord>(newSize->right  - newSize->left, 1));
    const auto h = static_cast<unsigned>(
        std::max<Steinberg::CCoord>(newSize->bottom - newSize->top,  1));
    XResizeWindow(x11Display, x11Win, w, h);
  }
  return view->onSize(newSize);
}
#else
Steinberg::tresult MinimalPlugFrame::resizeView(Steinberg::IPlugView *view,
                                                Steinberg::ViewRect *newSize) {
  if (!view || !newSize)
    return Steinberg::kInvalidArgument;
  return view->onSize(newSize);
}
#endif

struct Vst3PluginInstance {
  ~Vst3PluginInstance() { reset(); }

  void reset() noexcept {
    // Fecha o editor ImGui antes de liberar o controller (garante join da thread)
    closeImGuiFallbackEditor(imguiEditor);
    editorWindow.close();

    if (editController) {
      if (editControllerInitializedByHost)
        editController->terminate();
      editController->release();
      editController = nullptr;
      editControllerInitializedByHost = false;
    }
    componentHandler.reset();

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
    parameters.clear();
    displayName.clear();
  }

  std::unique_ptr<SharedLibrary> library;
  Steinberg::IPluginFactory *factory = nullptr;
  Steinberg::Vst::IComponent *component = nullptr;
  Steinberg::Vst::IAudioProcessor *audioProcessor = nullptr;
  Steinberg::Vst::IEditController *editController = nullptr;
  bool editControllerInitializedByHost = false;
  std::unique_ptr<MinimalComponentHandler> componentHandler;
  Vst3EditorWindow editorWindow;
  std::string pluginPath;
  std::string componentClassId;
  Steinberg::int32 channelCount = 0;
  Steinberg::Vst::SampleRate sampleRate = 0.0;
  Steinberg::int32 maxSamplesPerBlock = 0;
  // Campos adicionados para bridging de parâmetros e fallback ImGui
  std::vector<CachedParameterInfo>  parameters;
  std::string                        displayName;
  Vst3ImGuiEditorState              *imguiEditor = nullptr;
};

struct AudioDspVst3FilterState {
  obs_source_t *source = nullptr;
  std::atomic_bool bypass{true};
  std::atomic<float> linearGain{1.0F};
  std::atomic_bool backendReloadRequested{false};
  std::atomic_bool realtimeProcessorBlocked{false};
  std::atomic<std::uint32_t> activeAudioCallbacks{0};
  std::atomic<Steinberg::Vst::IAudioProcessor *> realtimeAudioProcessor{
      nullptr};
  std::atomic<Steinberg::int32> realtimeChannelCount{0};
  std::mutex selectedPluginMutex;
  std::mutex vst3InstanceMutex;
  std::string selectedPluginIdentifier;
  std::unique_ptr<MinimalVst3HostContext> hostContext{
      std::make_unique<MinimalVst3HostContext>()};
  Vst3PluginInstance vst3Instance;
};

// Declaração antecipada — implementação completa após os helpers de janela nativa.
// Necessária porque initializeSelectedVst3PluginLocked chama esta função.
void populateParameterCache(Vst3PluginInstance &instance);

const char *getName(void *) { return kPluginDisplayName; }

class RealtimeAudioCallbackScope {
public:
  explicit RealtimeAudioCallbackScope(AudioDspVst3FilterState &state) noexcept
      : state_(state) {
    state_.activeAudioCallbacks.fetch_add(1, std::memory_order_acq_rel);
  }

  RealtimeAudioCallbackScope(const RealtimeAudioCallbackScope &) = delete;
  RealtimeAudioCallbackScope &
  operator=(const RealtimeAudioCallbackScope &) = delete;

  ~RealtimeAudioCallbackScope() {
    state_.activeAudioCallbacks.fetch_sub(1, std::memory_order_acq_rel);
  }

  [[nodiscard]] bool isBlocked() const noexcept {
    return state_.realtimeProcessorBlocked.load(std::memory_order_acquire);
  }

private:
  AudioDspVst3FilterState &state_;
};

void blockRealtimeVst3Processing(AudioDspVst3FilterState &state) noexcept {
  state.realtimeProcessorBlocked.store(true, std::memory_order_release);
  state.realtimeAudioProcessor.store(nullptr, std::memory_order_release);
  state.realtimeChannelCount.store(0, std::memory_order_release);

  while (state.activeAudioCallbacks.load(std::memory_order_acquire) != 0)
    std::this_thread::yield();
}

void publishRealtimeVst3Processor(AudioDspVst3FilterState &state) noexcept {
  state.realtimeChannelCount.store(state.vst3Instance.channelCount,
                                   std::memory_order_release);
  state.realtimeAudioProcessor.store(state.vst3Instance.audioProcessor,
                                     std::memory_order_release);
  state.realtimeProcessorBlocked.store(false, std::memory_order_release);
}

void publishRealtimePassthrough(AudioDspVst3FilterState &state) noexcept {
  state.realtimeAudioProcessor.store(nullptr, std::memory_order_release);
  state.realtimeChannelCount.store(0, std::memory_order_release);
  state.realtimeProcessorBlocked.store(false, std::memory_order_release);
}

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

[[nodiscard]] bool isEmptyTuid(const Steinberg::TUID classId) noexcept {
  Steinberg::TUID empty = {};
  return tuidEquals(classId, empty);
}

void initializeVst3EditControllerLocked(Vst3PluginInstance &instance,
                                        MinimalVst3HostContext &hostContext,
                                        obs_source_t *source) {
  if (!instance.component || !instance.factory)
    return;

  void *controllerObject = nullptr;
  if (instance.component->queryInterface(
          INLINE_UID_OF(Steinberg::Vst::IEditController), &controllerObject) ==
          Steinberg::kResultOk &&
      controllerObject) {
    instance.editController =
        static_cast<Steinberg::Vst::IEditController *>(controllerObject);
  } else {
    Steinberg::TUID controllerClassId = {};
    if (instance.component->getControllerClassId(controllerClassId) !=
            Steinberg::kResultOk ||
        isEmptyTuid(controllerClassId)) {
      blog(LOG_INFO,
           "AUDIO_DSP VST3: selected plugin does not expose a VST3 edit "
           "controller");
      return;
    }

    if (instance.factory->createInstance(
            controllerClassId, INLINE_UID_OF(Steinberg::Vst::IEditController),
            &controllerObject) != Steinberg::kResultOk ||
        !controllerObject) {
      blog(LOG_WARNING,
           "AUDIO_DSP VST3: failed to create VST3 edit controller");
      return;
    }

    instance.editController =
        static_cast<Steinberg::Vst::IEditController *>(controllerObject);
    if (instance.editController->initialize(&hostContext) !=
        Steinberg::kResultOk) {
      blog(LOG_WARNING,
           "AUDIO_DSP VST3: failed to initialize VST3 edit controller");
      instance.editController->release();
      instance.editController = nullptr;
      instance.editControllerInitializedByHost = false;
      return;
    }
    instance.editControllerInitializedByHost = true;
  }

  instance.componentHandler = std::make_unique<MinimalComponentHandler>(source);
  static_cast<void>(instance.editController->setComponentHandler(
      instance.componentHandler.get()));
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

  // Resolve the component class ID: use the one from cache if available,
  // otherwise auto-discover the first audio processor class in the factory.
  Steinberg::TUID componentClassId = {};
  if (!selectedEntry->classId.empty()) {
    if (!parseVst3ClassId(selectedEntry->classId, componentClassId)) {
      logVst3LoadError("selected VST3 cache entry contains an invalid class ID",
                       selectedEntry->path);
      state.vst3Instance.reset();
      return false;
    }
  } else {
    constexpr std::string_view kAudioModuleClass = "Audio Module Class";
    const Steinberg::int32 classCount =
        state.vst3Instance.factory->countClasses();
    bool foundClass = false;
    for (Steinberg::int32 i = 0; i < classCount && !foundClass; ++i) {
      Steinberg::PClassInfo classInfo = {};
      if (state.vst3Instance.factory->getClassInfo(i, &classInfo) !=
          Steinberg::kResultOk)
        continue;
      if (std::string_view(classInfo.category) == kAudioModuleClass) {
        std::memcpy(componentClassId, classInfo.cid, sizeof(Steinberg::TUID));
        foundClass = true;
      }
    }
    if (!foundClass) {
      logVst3LoadError(
          "could not auto-discover audio component class in VST3 binary",
          binaryPath.string());
      state.vst3Instance.reset();
      return false;
    }
    blog(LOG_INFO,
         "AUDIO_DSP VST3: auto-discovered component class in: %s",
         binaryPath.string().c_str());
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

  initializeVst3EditControllerLocked(state.vst3Instance, *state.hostContext,
                                      state.source);

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

  {
    const Steinberg::tresult busResult =
        state.vst3Instance.audioProcessor->setBusArrangements(
            inputArrangements.data(), inputBusCount, outputArrangements.data(),
            outputBusCount);
    if (busResult != Steinberg::kResultOk &&
        busResult != Steinberg::kResultFalse) {
      logVst3LoadError("VST3 bus arrangement setup failed",
                       selectedEntry->path);
      state.vst3Instance.reset();
      return false;
    }
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

  {
    const Steinberg::tresult processingResult =
        state.vst3Instance.audioProcessor->setProcessing(true);
    if (processingResult != Steinberg::kResultOk &&
        processingResult != Steinberg::kResultFalse) {
      logVst3LoadError("VST3 processing activation failed",
                       selectedEntry->path);
      state.vst3Instance.reset();
      return false;
    }
  }

  state.vst3Instance.pluginPath = selectedEntry->path;
  state.vst3Instance.componentClassId = selectedEntry->classId;
  state.vst3Instance.channelCount = audioConfiguration.channelCount;
  state.vst3Instance.sampleRate = audioConfiguration.sampleRate;
  state.vst3Instance.maxSamplesPerBlock = audioConfiguration.maxSamplesPerBlock;

  // Constrói cache de parâmetros para bridging com o painel OBS
  populateParameterCache(state.vst3Instance);
  state.vst3Instance.displayName =
      selectedEntry->name.empty() ? selectedEntry->path : selectedEntry->name;

  blog(LOG_INFO, "AUDIO_DSP VST3: loaded VST3 plugin: %s (%zu parâmetro(s))",
       selectedEntry->path.c_str(), state.vst3Instance.parameters.size());
  return true;
}

void reloadSelectedVst3Plugin(AudioDspVst3FilterState &state,
                              std::string_view selectedIdentifier) {
  std::lock_guard<std::mutex> lock(state.vst3InstanceMutex);
  blockRealtimeVst3Processing(state);

  if (selectedIdentifier.empty()) {
    state.vst3Instance.reset();
    publishRealtimePassthrough(state);
    state.backendReloadRequested.store(false, std::memory_order_release);
    return;
  }

  const bool loaded =
      initializeSelectedVst3PluginLocked(state, selectedIdentifier);
  if (loaded)
    publishRealtimeVst3Processor(state);
  else
    publishRealtimePassthrough(state);

  state.backendReloadRequested.store(false, std::memory_order_release);
}

#if defined(_WIN32)
[[nodiscard]] bool openVst3EditorWindowLocked(Vst3PluginInstance &instance) {
  if (!instance.editController) {
    blog(LOG_WARNING,
         "AUDIO_DSP VST3: selected plugin does not provide a native editor");
    return false;
  }

  if (instance.editorWindow.hwnd) {
    ShowWindow(instance.editorWindow.hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(instance.editorWindow.hwnd);
    return true;
  }

  instance.editorWindow.close();
  instance.editorWindow.plugView =
      instance.editController->createView(Steinberg::Vst::ViewType::kEditor);
  if (!instance.editorWindow.plugView) {
    blog(LOG_WARNING, "AUDIO_DSP VST3: VST3 editor view creation failed");
    return false;
  }

  if (instance.editorWindow.plugView->isPlatformTypeSupported(
          Steinberg::kPlatformTypeHWND) != Steinberg::kResultOk) {
    blog(LOG_WARNING,
         "AUDIO_DSP VST3: VST3 editor does not support HWND embedding");
    instance.editorWindow.close();
    return false;
  }

  Steinberg::ViewRect viewSize = {0, 0, 640, 480};
  static_cast<void>(instance.editorWindow.plugView->getSize(&viewSize));

  if (!ensureEditorWindowClassRegistered()) {
    blog(LOG_ERROR,
         "AUDIO_DSP VST3: failed to register native editor window class");
    instance.editorWindow.close();
    return false;
  }

  RECT windowRect = {0, 0, viewRectWidth(viewSize), viewRectHeight(viewSize)};
  AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0);

  instance.editorWindow.hwnd = CreateWindowExW(
      0, kEditorWindowClassName, L"AUDIO_DSP VST3 Plugin Interface",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
      windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
      nullptr, nullptr, GetModuleHandleW(nullptr), &instance.editorWindow);
  if (!instance.editorWindow.hwnd) {
    blog(LOG_ERROR, "AUDIO_DSP VST3: failed to create native editor window");
    instance.editorWindow.close();
    return false;
  }

  instance.editorWindow.plugFrame.hwnd = instance.editorWindow.hwnd;
  static_cast<void>(instance.editorWindow.plugView->setFrame(
      &instance.editorWindow.plugFrame));

  if (instance.editorWindow.plugView->attached(
          static_cast<void *>(instance.editorWindow.hwnd),
          Steinberg::kPlatformTypeHWND) != Steinberg::kResultOk) {
    blog(LOG_WARNING, "AUDIO_DSP VST3: failed to attach VST3 editor view");
    instance.editorWindow.close();
    return false;
  }

  instance.editorWindow.attached = true;
  static_cast<void>(instance.editorWindow.plugView->onSize(&viewSize));
  ShowWindow(instance.editorWindow.hwnd, SW_SHOWNORMAL);
  UpdateWindow(instance.editorWindow.hwnd);
  return true;
}
#elif defined(__linux__)
[[nodiscard]] bool openVst3EditorWindowLocked(Vst3PluginInstance &instance) {
  if (!instance.editController) {
    blog(LOG_WARNING, "AUDIO_DSP VST3: plugin sem edit controller");
    return false;
  }

  // Se janela X11 já está aberta, traz para frente
  if (instance.editorWindow.x11Win && instance.editorWindow.x11Display) {
    XRaiseWindow(instance.editorWindow.x11Display, instance.editorWindow.x11Win);
    XFlush(instance.editorWindow.x11Display);
    return true;
  }

  instance.editorWindow.close();

  instance.editorWindow.plugView =
      instance.editController->createView(Steinberg::Vst::ViewType::kEditor);
  if (!instance.editorWindow.plugView) {
    blog(LOG_INFO,
         "AUDIO_DSP VST3: plugin não tem IPlugView — caller tentará editor ImGui fallback");
    return false;
  }

  if (instance.editorWindow.plugView->isPlatformTypeSupported(
          Steinberg::kPlatformTypeX11EmbedWindowID) != Steinberg::kResultOk) {
    blog(LOG_WARNING, "AUDIO_DSP VST3: plugin não suporta X11EmbedWindowID");
    instance.editorWindow.close();
    return false;
  }

  Display *dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    blog(LOG_ERROR, "AUDIO_DSP VST3: XOpenDisplay falhou");
    instance.editorWindow.close();
    return false;
  }

  Steinberg::ViewRect viewSize = {0, 0, 640, 480};
  static_cast<void>(instance.editorWindow.plugView->getSize(&viewSize));
  const unsigned w = static_cast<unsigned>(
      std::max<Steinberg::CCoord>(viewSize.right - viewSize.left, 64));
  const unsigned h = static_cast<unsigned>(
      std::max<Steinberg::CCoord>(viewSize.bottom - viewSize.top, 64));

  XSetWindowAttributes swa = {};
  swa.event_mask = StructureNotifyMask;
  Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
                                   0, 0, w, h, 0,
                                   BlackPixel(dpy, DefaultScreen(dpy)),
                                   BlackPixel(dpy, DefaultScreen(dpy)));
  XChangeWindowAttributes(dpy, win, CWEventMask, &swa);

  const std::string title = "AUDIO_DSP VST3 — " + instance.displayName;
  XStoreName(dpy, win, title.c_str());

  Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wmDelete, 1);
  XMapRaised(dpy, win);
  XFlush(dpy);

  instance.editorWindow.x11Display           = dpy;
  instance.editorWindow.x11Win               = win;
  instance.editorWindow.plugFrame.x11Display = dpy;
  instance.editorWindow.plugFrame.x11Win     = win;

  static_cast<void>(
      instance.editorWindow.plugView->setFrame(&instance.editorWindow.plugFrame));

  if (instance.editorWindow.plugView->attached(
          reinterpret_cast<void *>(static_cast<std::uintptr_t>(win)),
          Steinberg::kPlatformTypeX11EmbedWindowID) != Steinberg::kResultOk) {
    blog(LOG_WARNING, "AUDIO_DSP VST3: attached() falhou para X11EmbedWindowID");
    instance.editorWindow.close();
    return false;
  }

  instance.editorWindow.attached = true;
  static_cast<void>(instance.editorWindow.plugView->onSize(&viewSize));

  // Thread de eventos X11 — fecha a janela quando o usuário clica no botão X
  const Atom capturedWmDelete = wmDelete;
  instance.editorWindow.x11EventThreadRunning.store(true, std::memory_order_release);
  instance.editorWindow.x11EventThread = std::thread(
      [&ew = instance.editorWindow, capturedWmDelete]() {
        Display *d = ew.x11Display;
        while (ew.x11EventThreadRunning.load(std::memory_order_acquire)) {
          while (XPending(d) > 0) {
            XEvent ev;
            XNextEvent(d, &ev);
            if (ev.type == ClientMessage &&
                static_cast<Atom>(ev.xclient.data.l[0]) == capturedWmDelete) {
              ew.x11EventThreadRunning.store(false, std::memory_order_release);
              return;
            }
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
      });

  blog(LOG_INFO, "AUDIO_DSP VST3: janela X11 nativa aberta para '%s'",
       instance.displayName.c_str());
  return true;
}
#else
[[nodiscard]] bool openVst3EditorWindowLocked(Vst3PluginInstance &instance) {
  if (!instance.editController) {
    blog(LOG_WARNING,
         "AUDIO_DSP VST3: selected plugin does not provide a native editor");
    return false;
  }
  blog(LOG_WARNING,
       "AUDIO_DSP VST3: native VST3 editor windows not supported on this platform");
  return false;
}
#endif

// ─── Helpers: bridging de parâmetros VST3 ↔ painel OBS ───────────────────────

[[nodiscard]] std::string paramPropertyKey(Steinberg::Vst::ParamID id) {
  return std::string(kVst3ParamKeyPrefix) + std::to_string(id);
}

// Constrói a lista de parâmetros a partir do IEditController.
void populateParameterCache(Vst3PluginInstance &instance) {
  instance.parameters.clear();
  auto *ctrl = instance.editController;
  if (!ctrl) return;

  const Steinberg::int32 count = ctrl->getParameterCount();
  instance.parameters.reserve(static_cast<std::size_t>(count));

  for (Steinberg::int32 i = 0; i < count; ++i) {
    Steinberg::Vst::ParameterInfo info = {};
    if (ctrl->getParameterInfo(i, info) != Steinberg::kResultOk) continue;
    // Ignora parâmetros de mudança de programa
    if (info.flags & Steinberg::Vst::ParameterInfo::kIsProgramChange) continue;

    CachedParameterInfo param;
    param.id                = info.id;
    param.title             = detail::tcharToUtf8(info.title);
    param.units             = detail::tcharToUtf8(info.units);
    param.stepCount         = info.stepCount;
    param.defaultNormalized = info.defaultNormalizedValue;
    param.flags             = info.flags;
    instance.parameters.push_back(std::move(param));
  }
}

// Adiciona propriedades OBS para cada parâmetro do plugin carregado.
void addParamProperties(obs_properties_t *props,
                         const Vst3PluginInstance &instance) {
  auto *ctrl = instance.editController;
  if (!ctrl || instance.parameters.empty()) return;

  for (const auto &param : instance.parameters) {
    using PF = Steinberg::Vst::ParameterInfo;
    if (param.flags & PF::kIsHidden) continue;

    const std::string key = paramPropertyKey(param.id);
    const std::string label =
        param.title + (param.units.empty() ? "" : " (" + param.units + ")");

    if (param.stepCount == 0) {
      // Contínuo — slider normalizado 0..1
      obs_properties_add_float_slider(props, key.c_str(), label.c_str(),
                                      0.0, 1.0, 0.001);
    } else if (param.stepCount == 1) {
      // Toggle
      obs_properties_add_bool(props, key.c_str(), label.c_str());
    } else {
      // Discreto — lista com strings de display do próprio plugin
      obs_property_t *list =
          obs_properties_add_list(props, key.c_str(), label.c_str(),
                                  OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
      for (int step = 0; step <= param.stepCount; ++step) {
        const double normVal =
            static_cast<double>(step) / static_cast<double>(param.stepCount);
        Steinberg::Vst::String128 displayStr = {};
        std::string display;
        if (ctrl->getParamStringByValue(param.id, normVal, displayStr) ==
            Steinberg::kResultOk)
          display = detail::tcharToUtf8(displayStr);
        else
          display = std::to_string(step);
        obs_property_list_add_int(list, display.c_str(), step);
      }
    }
  }
}

// Lê valores normalizados atuais do VST3 e grava no obs_data (para sliders exibirem
// os valores corretos quando o painel OBS é aberto).
void syncParamsFromVst3(obs_data_t *settings,
                         const Vst3PluginInstance &instance) {
  auto *ctrl = instance.editController;
  if (!ctrl || instance.parameters.empty()) return;

  for (const auto &param : instance.parameters) {
    const std::string key  = paramPropertyKey(param.id);
    const double      norm = ctrl->getParamNormalized(param.id);

    if (param.stepCount == 1) {
      obs_data_set_bool(settings, key.c_str(), norm >= 0.5);
    } else if (param.stepCount == 0) {
      obs_data_set_double(settings, key.c_str(), norm);
    } else {
      const int step =
          static_cast<int>(std::round(norm * static_cast<double>(param.stepCount)));
      obs_data_set_int(settings, key.c_str(),
                       std::clamp(step, 0, param.stepCount));
    }
  }
}

// Lê valores do obs_data e os empurra para o IEditController do plugin.
void pushParamsToController(obs_data_t *settings,
                             const Vst3PluginInstance &instance) {
  auto *ctrl = instance.editController;
  if (!ctrl || instance.parameters.empty()) return;

  for (const auto &param : instance.parameters) {
    using PF = Steinberg::Vst::ParameterInfo;
    if (param.flags & PF::kIsReadOnly) continue;

    const std::string key = paramPropertyKey(param.id);
    double norm = 0.0;

    if (param.stepCount == 1) {
      norm = obs_data_get_bool(settings, key.c_str()) ? 1.0 : 0.0;
    } else if (param.stepCount == 0) {
      norm = std::clamp(obs_data_get_double(settings, key.c_str()), 0.0, 1.0);
    } else {
      const auto step =
          static_cast<int>(obs_data_get_int(settings, key.c_str()));
      norm = static_cast<double>(std::clamp(step, 0, param.stepCount)) /
             static_cast<double>(param.stepCount);
    }

    static_cast<void>(ctrl->setParamNormalized(param.id, norm));
  }
}

// Callback: usuário selecionou outro plugin no dropdown.
// Recarrega o plugin e pede reconstrução do painel OBS.
bool onPluginSelectionModified(obs_properties_t *props, obs_property_t *,
                                obs_data_t *settings) {
  auto *state =
      static_cast<AudioDspVst3FilterState *>(obs_properties_get_param(props));
  if (!state) return true;

  const char *selected =
      obs_data_get_string(settings, kVst3PluginSelectionSetting);
  const std::string identifier = selected ? selected : "";

  {
    std::lock_guard<std::mutex> lock(state->selectedPluginMutex);
    state->selectedPluginIdentifier = identifier;
  }

  reloadSelectedVst3Plugin(*state, identifier);
  // Reconstrói o painel de propriedades para mostrar/esconder parâmetros do plugin
  obs_source_update_properties(state->source);
  return true;
}

// Callback: botão "Scan VST3 Plugins".
// Escaneia diretórios e repopula o dropdown sem fechar o painel.
bool onScanPluginsClicked(obs_properties_t *props, obs_property_t *,
                           void * /*data*/) {
  scanAndCacheVst3Plugins();

  obs_property_t *pluginList =
      obs_properties_get(props, kVst3PluginSelectionSetting);
  if (pluginList) {
    obs_property_list_clear(pluginList);
    populateVst3PluginList(pluginList);
  }

  return true;
}

// ─── Abertura do editor do plugin ─────────────────────────────────────────────

bool openPluginInterfaceClicked(obs_properties_t *, obs_property_t *,
                                void *data) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  if (!state)
    return false;

  std::lock_guard<std::mutex> lock(state->vst3InstanceMutex);

  // Tenta janela nativa (HWND no Windows, X11EmbedWindowID no Linux)
  if (openVst3EditorWindowLocked(state->vst3Instance))
    return true;

  // Fallback Dear ImGui: usado quando o plugin não tem IPlugView
  Vst3PluginInstance &inst = state->vst3Instance;
  if (!inst.editController || inst.parameters.empty())
    return false;

  // Se o editor ImGui já está aberto, não abre outro
  if (inst.imguiEditor)
    return true;

  inst.imguiEditor = openImGuiFallbackEditor(
      inst.displayName.c_str(), inst.parameters, inst.editController);

  return inst.imguiEditor != nullptr;
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

  if (selectedPluginChanged) {
    reloadSelectedVst3Plugin(*state, selectedPluginIdentifier);
  } else {
    // Plugin não mudou — empurra alterações do painel OBS para o VST3
    std::lock_guard<std::mutex> lock(state->vst3InstanceMutex);
    pushParamsToController(settings, state->vst3Instance);
  }
}

void *create(obs_data_t *settings, obs_source_t *source) {
  auto *state = new AudioDspVst3FilterState;
  state->source = source;
  update(state, settings);
  return state;
}

void destroy(void *data) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  if (!state)
    return;

  {
    std::lock_guard<std::mutex> lock(state->vst3InstanceMutex);
    blockRealtimeVst3Processing(*state);
    state->vst3Instance.reset();
    publishRealtimePassthrough(*state);
  }

  delete state;
}

void getDefaults(obs_data_t *settings) {
  obs_data_set_default_bool(settings, kBypassSetting, false);
  obs_data_set_default_double(settings, kGainDbSetting, kDefaultGainDb);
  obs_data_set_default_string(settings, kVst3PluginSelectionSetting, "");
}

obs_properties_t *getProperties(void *data) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  obs_properties_t *properties = obs_properties_create();
  // Armazena o state para uso nos callbacks (onPluginSelectionModified, etc.)
  obs_properties_set_param(properties, state, nullptr);

  obs_properties_add_bool(properties, kBypassSetting, kBypassLabel);

  obs_property_t *pluginList = obs_properties_add_list(
      properties, kVst3PluginSelectionSetting, kVst3PluginSelectionLabel,
      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  populateVst3PluginList(pluginList);
  // Recarrega plugin e reconstrói painel quando o usuário muda a seleção
  obs_property_set_modified_callback(pluginList, onPluginSelectionModified);

  obs_properties_add_float_slider(properties, kGainDbSetting, kGainDbLabel,
                                  kMinGainDb, kMaxGainDb, kGainDbStep);
  obs_properties_add_button(properties, kScanPluginsButton, kScanPluginsLabel,
                            onScanPluginsClicked);
  obs_properties_add_button(properties, kOpenPluginEditorButton,
                            kOpenPluginEditorLabel, openPluginInterfaceClicked);

  // Adiciona controles dos parâmetros do plugin VST3 carregado
  if (state) {
    std::lock_guard<std::mutex> lock(state->vst3InstanceMutex);
    if (state->vst3Instance.editController) {
      addParamProperties(properties, state->vst3Instance);
      // Sincroniza valores atuais do VST3 → obs_data para sliders exibirem valores corretos
      if (obs_data_t *settings = obs_source_get_settings(state->source)) {
        syncParamsFromVst3(settings, state->vst3Instance);
        obs_data_release(settings);
      }
    }
  }

  return properties;
}

obs_audio_data *filterAudio(void *data, obs_audio_data *audio) {
  auto *state = static_cast<AudioDspVst3FilterState *>(data);
  if (!state)
    return audio;

  if (!audio)
    return nullptr;

  if (state->bypass.load(std::memory_order_relaxed))
    return audio;

  RealtimeAudioCallbackScope realtimeScope(*state);
  if (realtimeScope.isBlocked())
    return audio;

  ObsAudioBufferAdapter bufferAdapter(audio);
  if (!bufferAdapter.isValid())
    return audio;

  // Apply internal gain stage (always active when bypass is off)
  const float linearGain = state->linearGain.load(std::memory_order_relaxed);
  if (linearGain != 1.0F) {
    InternalGainProcessor gainProcessor;
    gainProcessor.process(bufferAdapter.audioView(), linearGain);
  }

  Steinberg::Vst::IAudioProcessor *audioProcessor =
      state->realtimeAudioProcessor.load(std::memory_order_acquire);
  if (!audioProcessor)
    return audio;

  const std::size_t channelCount = bufferAdapter.numChannels();
  const std::size_t frameCount = bufferAdapter.numFrames();
  const Steinberg::int32 expectedChannelCount =
      state->realtimeChannelCount.load(std::memory_order_acquire);
  if (expectedChannelCount <= 0 ||
      channelCount != static_cast<std::size_t>(expectedChannelCount) ||
      channelCount > ObsAudioBufferAdapter::kMaxChannels ||
      frameCount > static_cast<std::size_t>(
                       std::numeric_limits<Steinberg::int32>::max()))
    return audio;

  auto audioView = bufferAdapter.audioView();
  std::array<Steinberg::Vst::Sample32 *, ObsAudioBufferAdapter::kMaxChannels>
      channelBuffers{};
  for (std::size_t channel = 0; channel < channelCount; ++channel)
    channelBuffers[channel] = audioView.getChannel(channel);

  Steinberg::Vst::AudioBusBuffers inputBus = {};
  inputBus.numChannels = expectedChannelCount;
  inputBus.silenceFlags = 0;
  inputBus.channelBuffers32 = channelBuffers.data();

  Steinberg::Vst::AudioBusBuffers outputBus = {};
  outputBus.numChannels = expectedChannelCount;
  outputBus.silenceFlags = 0;
  outputBus.channelBuffers32 = channelBuffers.data();

  Steinberg::Vst::ProcessData processData = {};
  processData.processMode = Steinberg::Vst::kRealtime;
  processData.symbolicSampleSize = Steinberg::Vst::kSample32;
  processData.numSamples = static_cast<Steinberg::int32>(frameCount);
  processData.numInputs = 1;
  processData.numOutputs = 1;
  processData.inputs = &inputBus;
  processData.outputs = &outputBus;

  static_cast<void>(audioProcessor->process(processData));
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
