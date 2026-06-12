#include <obs-module.h>

#include "CV_OBS_PLUGIN/AudioDspVst3Filter.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-audio-dsp-vst3", "pt-BR")

namespace
{
obs_source_info g_audioDspVst3FilterInfo = cv_obs_plugin::makeAudioDspVst3FilterInfo();
}

MODULE_EXPORT const char* obs_module_description(void)
{
    return "AUDIO_DSP native OBS audio filter prepared for VST3 hosting.";
}

bool obs_module_load(void)
{
    obs_register_source(&g_audioDspVst3FilterInfo);
    return true;
}
