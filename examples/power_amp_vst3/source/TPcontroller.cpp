//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if POWER_AMP_VST3_ENABLE_CV_GUI
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

tresult PLUGIN_API PowerAmpVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Input Gain"), kParamPowerAmpInputGain, STR16 (""),
                                                      0.0, 4.0, 1.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Model"), kParamPowerAmpModel, STR16 (""),
                                                      0.0, 3.0, 0.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Output Gain"), kParamPowerAmpOutputGain, STR16 (""),
                                                      0.0, 4.0, 1.0));
    return result;
}

tresult PLUGIN_API PowerAmpVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API PowerAmpVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float inputGain = 1.0f;
    float model = 0.0f;
    float outputGain = 1.0f;

    if (!streamer.readFloat (inputGain) || !streamer.readFloat (model) || !streamer.readFloat (outputGain))
        return kResultFalse;

    setParamNormalized (kParamPowerAmpInputGain, normalize (inputGain, 0.0, 4.0));
    setParamNormalized (kParamPowerAmpModel, normalize (model, 0.0, 3.0));
    setParamNormalized (kParamPowerAmpOutputGain, normalize (outputGain, 0.0, 4.0));
    return kResultOk;
}

tresult PLUGIN_API PowerAmpVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API PowerAmpVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API PowerAmpVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if POWER_AMP_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
