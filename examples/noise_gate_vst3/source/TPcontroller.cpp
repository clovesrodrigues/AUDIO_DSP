//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if NOISE_GATE_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kOpenDefault = -40.0;
constexpr double kCloseDefault = -45.0;
constexpr double kAttackDefault = 1.0;
constexpr double kHoldDefault = 50.0;
constexpr double kReleaseDefault = 30.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API NoiseGateVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Threshold Open"), kParamNoiseGateThresholdOpen, STR16 ("dBFS"),
                                                      -80.0, 0.0, kOpenDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Threshold Close"), kParamNoiseGateThresholdClose, STR16 ("dBFS"),
                                                      -90.0, 0.0, kCloseDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Attack"), kParamNoiseGateAttack, STR16 ("ms"),
                                                      0.1, 100.0, kAttackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Hold"), kParamNoiseGateHold, STR16 ("ms"),
                                                      0.0, 500.0, kHoldDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Release"), kParamNoiseGateRelease, STR16 ("ms"),
                                                      1.0, 1000.0, kReleaseDefault));
    return result;
}

tresult PLUGIN_API NoiseGateVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API NoiseGateVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float open = static_cast<float> (kOpenDefault);
    float close = static_cast<float> (kCloseDefault);
    float attack = static_cast<float> (kAttackDefault);
    float hold = static_cast<float> (kHoldDefault);
    float release = static_cast<float> (kReleaseDefault);

    if (!streamer.readFloat (open) || !streamer.readFloat (close) || !streamer.readFloat (attack) ||
        !streamer.readFloat (hold) || !streamer.readFloat (release))
        return kResultFalse;

    setParamNormalized (kParamNoiseGateThresholdOpen, normalize (open, -80.0, 0.0));
    setParamNormalized (kParamNoiseGateThresholdClose, normalize (close, -90.0, 0.0));
    setParamNormalized (kParamNoiseGateAttack, normalize (attack, 0.1, 100.0));
    setParamNormalized (kParamNoiseGateHold, normalize (hold, 0.0, 500.0));
    setParamNormalized (kParamNoiseGateRelease, normalize (release, 1.0, 1000.0));
    return kResultOk;
}

tresult PLUGIN_API NoiseGateVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API NoiseGateVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API NoiseGateVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if NOISE_GATE_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
