#include "CV_OBS_PLUGIN/AudioDspVst3Filter.hpp"
#include "CV_OBS_PLUGIN/InternalGainProcessor.hpp"
#include "CV_OBS_PLUGIN/ObsAudioBufferAdapter.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace cv_obs_plugin
{
namespace
{
constexpr const char* kBypassSetting = "bypass";
constexpr const char* kBypassLabel = "Bypass";
constexpr const char* kGainDbSetting = "internal_gain_db";
constexpr const char* kGainDbLabel = "Internal Gain (dB)";
constexpr double kMinGainDb = -24.0;
constexpr double kMaxGainDb = 24.0;
constexpr double kGainDbStep = 0.1;
constexpr double kDefaultGainDb = 0.0;

struct AudioDspVst3FilterState
{
    obs_source_t* source = nullptr;
    std::atomic_bool bypass{true};
    std::atomic<float> linearGain{1.0F};
    InternalGainProcessor gainProcessor;
};

const char* getName(void*)
{
    return kPluginDisplayName;
}

float gainDbToLinear(double gainDb) noexcept
{
    const double clampedGainDb = std::clamp(gainDb, kMinGainDb, kMaxGainDb);
    return static_cast<float>(std::pow(10.0, clampedGainDb / 20.0));
}

void update(void* data, obs_data_t* settings)
{
    auto* state = static_cast<AudioDspVst3FilterState*>(data);
    if (!state || !settings)
        return;

    state->bypass.store(obs_data_get_bool(settings, kBypassSetting), std::memory_order_relaxed);
    state->linearGain.store(
        gainDbToLinear(obs_data_get_double(settings, kGainDbSetting)),
        std::memory_order_relaxed);
}

void* create(obs_data_t* settings, obs_source_t* source)
{
    auto* state = new AudioDspVst3FilterState;
    state->source = source;
    update(state, settings);
    return state;
}

void destroy(void* data)
{
    auto* state = static_cast<AudioDspVst3FilterState*>(data);
    delete state;
}

void getDefaults(obs_data_t* settings)
{
    obs_data_set_default_bool(settings, kBypassSetting, true);
    obs_data_set_default_double(settings, kGainDbSetting, kDefaultGainDb);
}

obs_properties_t* getProperties(void*)
{
    obs_properties_t* properties = obs_properties_create();
    obs_properties_add_bool(properties, kBypassSetting, kBypassLabel);
    obs_properties_add_float_slider(
        properties,
        kGainDbSetting,
        kGainDbLabel,
        kMinGainDb,
        kMaxGainDb,
        kGainDbStep);
    return properties;
}

obs_audio_data* filterAudio(void* data, obs_audio_data* audio)
{
    auto* state = static_cast<AudioDspVst3FilterState*>(data);
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
        audioView,
        state->linearGain.load(std::memory_order_relaxed));
    return audio;
}
} // namespace

obs_source_info makeAudioDspVst3FilterInfo() noexcept
{
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
