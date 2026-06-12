#ifndef CV_OBS_PLUGIN_AUDIO_DSP_VST3_FILTER_HPP
#define CV_OBS_PLUGIN_AUDIO_DSP_VST3_FILTER_HPP

#include <obs-module.h>

namespace cv_obs_plugin
{

constexpr const char* kPluginId = "obs_audio_dsp_vst3_filter";
constexpr const char* kPluginDisplayName = "AUDIO_DSP VST3";

obs_source_info makeAudioDspVst3FilterInfo() noexcept;

} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_AUDIO_DSP_VST3_FILTER_HPP
