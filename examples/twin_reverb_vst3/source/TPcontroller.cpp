//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if TWIN_REVERB_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

#include <algorithm>

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kMixDefault = 0.25;
constexpr double kDwellDefault = 0.50;
constexpr double kToneDefault = 0.60;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API TwinReverbVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mix"), kParamMix, STR16 ("%"),
                                                      0.0, 1.0, kMixDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Dwell"), kParamDwell, STR16 ("%"),
                                                      0.0, 1.0, kDwellDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Tone"), kParamTone, STR16 ("%"),
                                                      0.0, 1.0, kToneDefault));
    return result;
}

tresult PLUGIN_API TwinReverbVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API TwinReverbVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mix = static_cast<float> (kMixDefault);
    float dwell = static_cast<float> (kDwellDefault);
    float tone = static_cast<float> (kToneDefault);

    if (!streamer.readFloat (mix) || !streamer.readFloat (dwell) || !streamer.readFloat (tone))
        return kResultFalse;

    setParamNormalized (kParamMix, normalize (std::clamp<double> (mix, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamDwell, normalize (std::clamp<double> (dwell, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamTone, normalize (std::clamp<double> (tone, 0.0, 1.0), 0.0, 1.0));
    return kResultOk;
}

tresult PLUGIN_API TwinReverbVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API TwinReverbVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API TwinReverbVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if TWIN_REVERB_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (Steinberg::ViewRect (0, 0, 720, 380), this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
