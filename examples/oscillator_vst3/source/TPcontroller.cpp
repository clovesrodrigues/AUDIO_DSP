//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if OSCILLATOR_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kWaveformDefault = 0.0;
constexpr double kFrequencyDefault = 440.0;
constexpr double kPhaseDefault = 0.0;
Vst::ParamValue normalize (double plain, double minPlain, double maxPlain) { return (plain - minPlain) / (maxPlain - minPlain); }
}

tresult PLUGIN_API OscillatorVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;
    auto* wave = new Vst::StringListParameter (STR16 ("Waveform"), kParamOscillatorWaveform);
    wave->appendString (STR16 ("Sine"));
    wave->appendString (STR16 ("Triangle"));
    wave->appendString (STR16 ("Saw"));
    wave->appendString (STR16 ("Square"));
    wave->setNormalized (0.0);
    parameters.addParameter (wave);
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Frequency"), kParamOscillatorFrequency, STR16 ("Hz"), 20.0, 20000.0, kFrequencyDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Phase"), kParamOscillatorPhase, STR16 ("deg"), 0.0, 360.0, kPhaseDefault));
    return result;
}

tresult PLUGIN_API OscillatorVST3Controller::terminate () { return EditControllerEx1::terminate (); }

tresult PLUGIN_API OscillatorVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float waveform = static_cast<float> (kWaveformDefault), frequency = static_cast<float> (kFrequencyDefault), phase = static_cast<float> (kPhaseDefault);
    if (!streamer.readFloat (waveform) || !streamer.readFloat (frequency) || !streamer.readFloat (phase))
        return kResultFalse;
    setParamNormalized (kParamOscillatorWaveform, normalize (waveform, 0.0, 3.0));
    setParamNormalized (kParamOscillatorFrequency, normalize (frequency, 20.0, 20000.0));
    setParamNormalized (kParamOscillatorPhase, normalize (phase, 0.0, 360.0));
    return kResultOk;
}

tresult PLUGIN_API OscillatorVST3Controller::setState (IBStream* /*state*/) { return kResultTrue; }
tresult PLUGIN_API OscillatorVST3Controller::getState (IBStream* /*state*/) { return kResultTrue; }

IPlugView* PLUGIN_API OscillatorVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;
#if OSCILLATOR_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif
    return nullptr;
}

} // namespace CV
