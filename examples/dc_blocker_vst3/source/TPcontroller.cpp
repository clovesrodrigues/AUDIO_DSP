//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if DC_BLOCKER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kCutoffDefault = 20.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API DCBlockerVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Cutoff"), kParamDCBlockerCutoff, STR16 ("Hz"),
                                                      1.0, 200.0, kCutoffDefault));
    return result;
}

tresult PLUGIN_API DCBlockerVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API DCBlockerVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float cutoff = static_cast<float> (kCutoffDefault);

    if (!streamer.readFloat (cutoff))
        return kResultFalse;

    setParamNormalized (kParamDCBlockerCutoff, normalize (cutoff, 1.0, 200.0));
    return kResultOk;
}

tresult PLUGIN_API DCBlockerVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API DCBlockerVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API DCBlockerVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if DC_BLOCKER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
