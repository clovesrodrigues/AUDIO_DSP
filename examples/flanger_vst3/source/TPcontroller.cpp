//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if FLANGER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kRateDefault = 0.25;
constexpr double kDepthDefault = 0.5;
constexpr double kFeedbackDefault = 0.25;
constexpr double kMixDefault = 0.5;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API FlangerVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Rate"), kParamFlangerRate, STR16 ("Hz"),
                                                      0.01, 20.0, kRateDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Depth"), kParamFlangerDepth, STR16 (""),
                                                      0.0, 1.0, kDepthDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Feedback"), kParamFlangerFeedback, STR16 (""),
                                                      -0.99, 0.99, kFeedbackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mix"), kParamFlangerMix, STR16 (""),
                                                      0.0, 1.0, kMixDefault));
    return result;
}

tresult PLUGIN_API FlangerVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API FlangerVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float rate = static_cast<float> (kRateDefault);
    float depth = static_cast<float> (kDepthDefault);
    float feedback = static_cast<float> (kFeedbackDefault);
    float mix = static_cast<float> (kMixDefault);

    if (!streamer.readFloat (rate) || !streamer.readFloat (depth) || !streamer.readFloat (feedback) || !streamer.readFloat (mix))
        return kResultFalse;

    setParamNormalized (kParamFlangerRate, normalize (rate, 0.01, 20.0));
    setParamNormalized (kParamFlangerDepth, normalize (depth, 0.0, 1.0));
    setParamNormalized (kParamFlangerFeedback, normalize (feedback, -0.99, 0.99));
    setParamNormalized (kParamFlangerMix, normalize (mix, 0.0, 1.0));
    return kResultOk;
}

tresult PLUGIN_API FlangerVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API FlangerVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API FlangerVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if FLANGER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
