#include "CV_OBS_PLUGIN/AudioDspVst3Filter.hpp"

namespace cv_obs_plugin
{
namespace
{
struct AudioDspVst3FilterState
{
    obs_source_t* source = nullptr;
};

const char* getName(void*)
{
    return kPluginDisplayName;
}

void* create(obs_data_t*, obs_source_t* source)
{
    auto* state = new AudioDspVst3FilterState;
    state->source = source;
    return state;
}

void destroy(void* data)
{
    auto* state = static_cast<AudioDspVst3FilterState*>(data);
    delete state;
}

obs_audio_data* filterAudio(void* data, obs_audio_data* audio)
{
    auto* state = static_cast<AudioDspVst3FilterState*>(data);
    if (!state || !audio)
        return audio;

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
    info.filter_audio = filterAudio;
    return info;
}

} // namespace cv_obs_plugin
