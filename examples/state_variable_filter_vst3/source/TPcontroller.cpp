//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if STATE_VARIABLE_FILTER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kModeDefault = 0.0;
constexpr double kCutoffDefault = 1000.0;
constexpr double kResonanceDefault = 0.707;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API StateVariableFilterVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* mode = new Vst::StringListParameter (STR16 ("Mode"), kParamSVFMode);
    mode->appendString (STR16 ("LowPass"));
    mode->appendString (STR16 ("HighPass"));
    mode->appendString (STR16 ("BandPass"));
    mode->appendString (STR16 ("Notch"));
    mode->setNormalized (0.0);
    parameters.addParameter (mode);
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Cutoff"), kParamSVFCutoff, STR16 ("Hz"), 20.0, 20000.0, kCutoffDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Resonance"), kParamSVFResonance, STR16 ("Q"), 0.1, 40.0, kResonanceDefault));
    return result;
}

tresult PLUGIN_API StateVariableFilterVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API StateVariableFilterVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mode = static_cast<float> (kModeDefault);
    float cutoff = static_cast<float> (kCutoffDefault);
    float resonance = static_cast<float> (kResonanceDefault);

    if (!streamer.readFloat (mode) || !streamer.readFloat (cutoff) || !streamer.readFloat (resonance))
        return kResultFalse;

    setParamNormalized (kParamSVFMode, normalize (mode, 0.0, 3.0));
    setParamNormalized (kParamSVFCutoff, normalize (cutoff, 20.0, 20000.0));
    setParamNormalized (kParamSVFResonance, normalize (resonance, 0.1, 40.0));
    return kResultOk;
}

tresult PLUGIN_API StateVariableFilterVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API StateVariableFilterVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API StateVariableFilterVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if STATE_VARIABLE_FILTER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
