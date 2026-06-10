//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if ENVELOPE_FOLLOWER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kAttackDefault = 10.0;
constexpr double kReleaseDefault = 100.0;
constexpr double kOutputGainDefault = 0.0;
constexpr double kMixDefault = 100.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

//------------------------------------------------------------------------
tresult PLUGIN_API EnvelopeFollowerVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* mode = new Vst::StringListParameter (STR16 ("Mode"), kParamEnvelopeMode);
    mode->appendString (STR16 ("Peak"));
    mode->appendString (STR16 ("RMS"));
    mode->setNormalized (0.0);
    parameters.addParameter (mode);

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Attack"), kParamEnvelopeAttack, STR16 ("ms"),
                                                      0.1, 200.0, kAttackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Release"), kParamEnvelopeRelease, STR16 ("ms"),
                                                      1.0, 2000.0, kReleaseDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Output Gain"), kParamEnvelopeOutputGain, STR16 ("dB"),
                                                      -24.0, 24.0, kOutputGainDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Wet Mix"), kParamEnvelopeMix, STR16 ("%"),
                                                      0.0, 100.0, kMixDefault));

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EnvelopeFollowerVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API EnvelopeFollowerVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    int32 mode = 0;
    float attack = static_cast<float> (kAttackDefault);
    float release = static_cast<float> (kReleaseDefault);
    float outputGain = static_cast<float> (kOutputGainDefault);
    float mix = static_cast<float> (kMixDefault);

    if (!streamer.readInt32 (mode) || !streamer.readFloat (attack) || !streamer.readFloat (release) ||
        !streamer.readFloat (outputGain) || !streamer.readFloat (mix))
        return kResultFalse;

    setParamNormalized (kParamEnvelopeMode, mode == kEnvelopeModeRMS ? 1.0 : 0.0);
    setParamNormalized (kParamEnvelopeAttack, normalize (attack, 0.1, 200.0));
    setParamNormalized (kParamEnvelopeRelease, normalize (release, 1.0, 2000.0));
    setParamNormalized (kParamEnvelopeOutputGain, normalize (outputGain, -24.0, 24.0));
    setParamNormalized (kParamEnvelopeMix, normalize (mix, 0.0, 100.0));
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EnvelopeFollowerVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EnvelopeFollowerVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API EnvelopeFollowerVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if ENVELOPE_FOLLOWER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
