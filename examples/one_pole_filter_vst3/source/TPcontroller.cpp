//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if ONE_POLE_FILTER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kModeDefault = 0.0;
constexpr double kCutoffDefault = 1000.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API OnePoleFilterVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* mode = new Vst::StringListParameter (STR16 ("Mode"), kParamOnePoleMode);
    mode->appendString (STR16 ("LowPass"));
    mode->appendString (STR16 ("HighPass"));
    mode->setNormalized (0.0);
    parameters.addParameter (mode);
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Cutoff"), kParamOnePoleCutoff, STR16 ("Hz"), 20.0, 20000.0, kCutoffDefault));
    return result;
}

tresult PLUGIN_API OnePoleFilterVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API OnePoleFilterVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mode = static_cast<float> (kModeDefault);
    float cutoff = static_cast<float> (kCutoffDefault);

    if (!streamer.readFloat (mode) || !streamer.readFloat (cutoff))
        return kResultFalse;

    setParamNormalized (kParamOnePoleMode, normalize (mode, 0.0, 1.0));
    setParamNormalized (kParamOnePoleCutoff, normalize (cutoff, 20.0, 20000.0));
    return kResultOk;
}

tresult PLUGIN_API OnePoleFilterVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API OnePoleFilterVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API OnePoleFilterVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if ONE_POLE_FILTER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
