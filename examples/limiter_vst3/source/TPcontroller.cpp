//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if LIMITER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kThresholdDefault = -0.3;
constexpr double kReleaseDefault = 50.0;
constexpr double kOutputGainDefault = 0.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

//------------------------------------------------------------------------
tresult PLUGIN_API LimiterVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Threshold"), kParamLimiterThreshold, STR16 ("dBFS"),
                                                      -24.0, 0.0, kThresholdDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Release"), kParamLimiterRelease, STR16 ("ms"),
                                                      1.0, 1000.0, kReleaseDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Output Gain"), kParamLimiterOutputGain, STR16 ("dB"),
                                                      -12.0, 12.0, kOutputGainDefault));
    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API LimiterVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API LimiterVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float threshold = static_cast<float> (kThresholdDefault);
    float release = static_cast<float> (kReleaseDefault);
    float outputGain = static_cast<float> (kOutputGainDefault);

    if (!streamer.readFloat (threshold) || !streamer.readFloat (release) || !streamer.readFloat (outputGain))
        return kResultFalse;

    setParamNormalized (kParamLimiterThreshold, normalize (threshold, -24.0, 0.0));
    setParamNormalized (kParamLimiterRelease, normalize (release, 1.0, 1000.0));
    setParamNormalized (kParamLimiterOutputGain, normalize (outputGain, -12.0, 12.0));
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API LimiterVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API LimiterVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API LimiterVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if LIMITER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
