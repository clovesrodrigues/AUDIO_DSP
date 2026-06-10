//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if AMP_SIMULATOR_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API AmpSimulatorVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Input Gain"), kParamAmpSimulatorInputGain, STR16 (""),
                                                      0.0, 4.0, 1.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Preamp Drive"), kParamAmpSimulatorPreampDrive, STR16 (""),
                                                      0.0, 30.0, 4.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bass"), kParamAmpSimulatorBass, STR16 (""),
                                                      0.0, 1.0, 0.5));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mid"), kParamAmpSimulatorMid, STR16 (""),
                                                      0.0, 1.0, 0.5));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Treble"), kParamAmpSimulatorTreble, STR16 (""),
                                                      0.0, 1.0, 0.5));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Presence"), kParamAmpSimulatorPresence, STR16 (""),
                                                      0.0, 1.0, 0.5));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Power Model"), kParamAmpSimulatorPowerModel, STR16 (""),
                                                      0.0, 3.0, 0.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Output Gain"), kParamAmpSimulatorOutputGain, STR16 (""),
                                                      0.0, 4.0, 1.0));
    return result;
}

tresult PLUGIN_API AmpSimulatorVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API AmpSimulatorVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float inputGain = 1.0f;
    float preampDrive = 4.0f;
    float bass = 0.5f;
    float mid = 0.5f;
    float treble = 0.5f;
    float presence = 0.5f;
    float powerModel = 0.0f;
    float outputGain = 1.0f;

    if (!streamer.readFloat (inputGain) || !streamer.readFloat (preampDrive) || !streamer.readFloat (bass) || !streamer.readFloat (mid) || !streamer.readFloat (treble) || !streamer.readFloat (presence) || !streamer.readFloat (powerModel) || !streamer.readFloat (outputGain))
        return kResultFalse;

    setParamNormalized (kParamAmpSimulatorInputGain, normalize (inputGain, 0.0, 4.0));
    setParamNormalized (kParamAmpSimulatorPreampDrive, normalize (preampDrive, 0.0, 30.0));
    setParamNormalized (kParamAmpSimulatorBass, normalize (bass, 0.0, 1.0));
    setParamNormalized (kParamAmpSimulatorMid, normalize (mid, 0.0, 1.0));
    setParamNormalized (kParamAmpSimulatorTreble, normalize (treble, 0.0, 1.0));
    setParamNormalized (kParamAmpSimulatorPresence, normalize (presence, 0.0, 1.0));
    setParamNormalized (kParamAmpSimulatorPowerModel, normalize (powerModel, 0.0, 3.0));
    setParamNormalized (kParamAmpSimulatorOutputGain, normalize (outputGain, 0.0, 4.0));
    return kResultOk;
}

tresult PLUGIN_API AmpSimulatorVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API AmpSimulatorVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API AmpSimulatorVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if AMP_SIMULATOR_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
