#ifndef CV_OBS_PLUGIN_VST3_STORAGE_MODEL_HPP
#define CV_OBS_PLUGIN_VST3_STORAGE_MODEL_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace cv_obs_plugin
{

namespace vst3_storage
{
constexpr const char* kRootDirectoryName = "obs-vst3";
constexpr const char* kPluginsDirectoryName = "plugins";
constexpr const char* kPresetsDirectoryName = "presets";
constexpr const char* kChainsDirectoryName = "chains";
constexpr const char* kCacheFileName = "cache.json";
constexpr const char* kBlacklistFileName = "blacklist.json";
constexpr std::uint32_t kCacheSchemaVersion = 1;
constexpr std::uint32_t kBlacklistSchemaVersion = 1;
} // namespace vst3_storage

enum class Vst3ScanStatus
{
    Unknown,
    CandidateFound,
    MetadataRead,
    Failed,
    Blacklisted,
};

struct Vst3PluginCacheEntry
{
    std::string path;
    std::string name;
    std::string vendor;
    std::string version;
    std::string classId;
    std::string category;
    Vst3ScanStatus status = Vst3ScanStatus::Unknown;
    std::uint64_t lastModifiedUnixSeconds = 0;
    std::uint64_t scannedAtUnixSeconds = 0;
};

struct Vst3PluginBlacklistEntry
{
    std::string path;
    std::string classId;
    std::string reason;
    std::string lastError;
    std::uint64_t failedAtUnixSeconds = 0;
    std::uint32_t failureCount = 0;
};

struct Vst3PluginCacheDocument
{
    std::uint32_t schemaVersion = vst3_storage::kCacheSchemaVersion;
    std::vector<Vst3PluginCacheEntry> plugins;
};

struct Vst3PluginBlacklistDocument
{
    std::uint32_t schemaVersion = vst3_storage::kBlacklistSchemaVersion;
    std::vector<Vst3PluginBlacklistEntry> plugins;
};

} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_VST3_STORAGE_MODEL_HPP
