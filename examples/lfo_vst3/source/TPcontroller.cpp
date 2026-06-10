//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if LFO_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kWaveformDefault = 0.0;
constexpr double kFrequencyDefault = 1.0;
Vst::ParamValue normalize (double plain, double minPlain, double maxPlain) { return (plain - minPlain) / (maxPlain - minPlain); }
}

tresult PLUGIN_API LFOVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;
    auto* wave = new Vst::StringListParameter (STR16 ("Waveform"), kParamLFOWaveform);
    wave->appendString (STR16 ("Sine"));
    wave->appendString (STR16 ("Triangle"));
    wave->appendString (STR16 ("Saw"));
    wave->appendString (STR16 ("Square"));
    wave->setNormalized (0.0);
    parameters.addParameter (wave);
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Frequency"), kParamLFOFrequency, STR16 ("Hz"), 0.01, 50.0, kFrequencyDefault));
    return result;
}

tresult PLUGIN_API LFOVST3Controller::terminate () { return EditControllerEx1::terminate (); }

tresult PLUGIN_API LFOVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float waveform = static_cast<float> (kWaveformDefault), frequency = static_cast<float> (kFrequencyDefault);
    if (!streamer.readFloat (waveform) || !streamer.readFloat (frequency))
        return kResultFalse;
    setParamNormalized (kParamLFOWaveform, normalize (waveform, 0.0, 3.0));
    setParamNormalized (kParamLFOFrequency, normalize (frequency, 0.01, 50.0));
    return kResultOk;
}

tresult PLUGIN_API LFOVST3Controller::setState (IBStream* /*state*/) { return kResultTrue; }
tresult PLUGIN_API LFOVST3Controller::getState (IBStream* /*state*/) { return kResultTrue; }

IPlugView* PLUGIN_API LFOVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;
#if LFO_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif
    return nullptr;
}

} // namespace CV
