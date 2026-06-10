//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if BIQUAD_FILTER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kModeDefault = 0.0;
constexpr double kFreqDefault = 1000.0;
constexpr double kQDefault = 0.707;
constexpr double kGainDefault = 0.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API BiquadFilterVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* mode = new Vst::StringListParameter (STR16 ("Mode"), kParamBiquadMode);
    mode->appendString (STR16 ("LowPass"));
    mode->appendString (STR16 ("HighPass"));
    mode->appendString (STR16 ("BandPass"));
    mode->appendString (STR16 ("Notch"));
    mode->appendString (STR16 ("AllPass"));
    mode->appendString (STR16 ("PeakingEQ"));
    mode->appendString (STR16 ("LowShelf"));
    mode->appendString (STR16 ("HighShelf"));
    mode->setNormalized (0.0);
    parameters.addParameter (mode);
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Frequency"), kParamBiquadFrequency, STR16 ("Hz"), 20.0, 20000.0, kFreqDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Q"), kParamBiquadQ, STR16 (""), 0.1, 20.0, kQDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Gain"), kParamBiquadGain, STR16 ("dB"), -24.0, 24.0, kGainDefault));
    return result;
}

tresult PLUGIN_API BiquadFilterVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API BiquadFilterVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mode = static_cast<float> (kModeDefault);
    float freq = static_cast<float> (kFreqDefault);
    float q = static_cast<float> (kQDefault);
    float gain = static_cast<float> (kGainDefault);

    if (!streamer.readFloat (mode) || !streamer.readFloat (freq) || !streamer.readFloat (q) || !streamer.readFloat (gain))
        return kResultFalse;

    setParamNormalized (kParamBiquadMode, normalize (mode, 0.0, 7.0));
    setParamNormalized (kParamBiquadFrequency, normalize (freq, 20.0, 20000.0));
    setParamNormalized (kParamBiquadQ, normalize (q, 0.1, 20.0));
    setParamNormalized (kParamBiquadGain, normalize (gain, -24.0, 24.0));
    return kResultOk;
}

tresult PLUGIN_API BiquadFilterVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API BiquadFilterVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API BiquadFilterVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if BIQUAD_FILTER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
