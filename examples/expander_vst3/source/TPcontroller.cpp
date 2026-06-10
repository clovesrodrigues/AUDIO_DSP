//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if EXPANDER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kThresholdDefault = -40.0;
constexpr double kRatioDefault = 4.0;
constexpr double kAttackDefault = 5.0;
constexpr double kReleaseDefault = 100.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

//------------------------------------------------------------------------
tresult PLUGIN_API ExpanderVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Threshold"), kParamExpanderThreshold, STR16 ("dB"),
                                                      -80.0, 0.0, kThresholdDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Ratio"), kParamExpanderRatio, STR16 (":1"),
                                                      1.0, 20.0, kRatioDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Attack"), kParamExpanderAttack, STR16 ("ms"),
                                                      0.1, 200.0, kAttackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Release"), kParamExpanderRelease, STR16 ("ms"),
                                                      1.0, 2000.0, kReleaseDefault));
    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ExpanderVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API ExpanderVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float threshold = static_cast<float> (kThresholdDefault);
    float ratio = static_cast<float> (kRatioDefault);
    float attack = static_cast<float> (kAttackDefault);
    float release = static_cast<float> (kReleaseDefault);

    if (!streamer.readFloat (threshold) || !streamer.readFloat (ratio) ||
        !streamer.readFloat (attack) || !streamer.readFloat (release))
        return kResultFalse;

    setParamNormalized (kParamExpanderThreshold, normalize (threshold, -80.0, 0.0));
    setParamNormalized (kParamExpanderRatio, normalize (ratio, 1.0, 20.0));
    setParamNormalized (kParamExpanderAttack, normalize (attack, 0.1, 200.0));
    setParamNormalized (kParamExpanderRelease, normalize (release, 1.0, 2000.0));
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ExpanderVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ExpanderVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API ExpanderVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if EXPANDER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
