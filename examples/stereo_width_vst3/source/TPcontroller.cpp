//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if STEREO_WIDTH_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kGainMin = 0.0;
constexpr double kGainMax = 2.0;
constexpr double kDefault = 1.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API StereoWidthVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mid Gain"), kParamStereoWidthVST3MidGain, STR16 (""),
                                                      kGainMin, kGainMax, kDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Side Gain"), kParamStereoWidthVST3SideGain, STR16 (""),
                                                      kGainMin, kGainMax, kDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Stereo Width"), kParamStereoWidthVST3Width, STR16 (""),
                                                      kGainMin, kGainMax, kDefault));
    return result;
}

tresult PLUGIN_API StereoWidthVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API StereoWidthVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float midGain = static_cast<float> (kDefault);
    float sideGain = static_cast<float> (kDefault);
    float width = static_cast<float> (kDefault);

    if (!streamer.readFloat (midGain) || !streamer.readFloat (sideGain) || !streamer.readFloat (width))
        return kResultFalse;

    setParamNormalized (kParamStereoWidthVST3MidGain, normalize (midGain, kGainMin, kGainMax));
    setParamNormalized (kParamStereoWidthVST3SideGain, normalize (sideGain, kGainMin, kGainMax));
    setParamNormalized (kParamStereoWidthVST3Width, normalize (width, kGainMin, kGainMax));
    return kResultOk;
}

tresult PLUGIN_API StereoWidthVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API StereoWidthVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API StereoWidthVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if STEREO_WIDTH_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
