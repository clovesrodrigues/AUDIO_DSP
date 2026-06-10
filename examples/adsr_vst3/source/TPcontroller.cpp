//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if ADSR_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kGateDefault = 0.0;
constexpr double kAttackDefault = 10.0;
constexpr double kDecayDefault = 100.0;
constexpr double kSustainDefault = 0.7;
constexpr double kReleaseDefault = 250.0;
Vst::ParamValue normalize (double plain, double minPlain, double maxPlain) { return (plain - minPlain) / (maxPlain - minPlain); }
}

tresult PLUGIN_API ADSRVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Gate"), kParamADSRGate, STR16 (""), 0.0, 1.0, kGateDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Attack"), kParamADSRAttack, STR16 ("ms"), 0.1, 5000.0, kAttackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Decay"), kParamADSRDecay, STR16 ("ms"), 0.1, 5000.0, kDecayDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Sustain"), kParamADSRSustain, STR16 (""), 0.0, 1.0, kSustainDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Release"), kParamADSRRelease, STR16 ("ms"), 0.1, 10000.0, kReleaseDefault));
    return result;
}

tresult PLUGIN_API ADSRVST3Controller::terminate () { return EditControllerEx1::terminate (); }

tresult PLUGIN_API ADSRVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float gate = static_cast<float> (kGateDefault), attack = static_cast<float> (kAttackDefault), decay = static_cast<float> (kDecayDefault), sustain = static_cast<float> (kSustainDefault), release = static_cast<float> (kReleaseDefault);
    if (!streamer.readFloat (gate) || !streamer.readFloat (attack) || !streamer.readFloat (decay) || !streamer.readFloat (sustain) || !streamer.readFloat (release))
        return kResultFalse;
    setParamNormalized (kParamADSRGate, normalize (gate, 0.0, 1.0));
    setParamNormalized (kParamADSRAttack, normalize (attack, 0.1, 5000.0));
    setParamNormalized (kParamADSRDecay, normalize (decay, 0.1, 5000.0));
    setParamNormalized (kParamADSRSustain, normalize (sustain, 0.0, 1.0));
    setParamNormalized (kParamADSRRelease, normalize (release, 0.1, 10000.0));
    return kResultOk;
}

tresult PLUGIN_API ADSRVST3Controller::setState (IBStream* /*state*/) { return kResultTrue; }
tresult PLUGIN_API ADSRVST3Controller::getState (IBStream* /*state*/) { return kResultTrue; }

IPlugView* PLUGIN_API ADSRVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;
#if ADSR_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif
    return nullptr;
}

} // namespace CV
