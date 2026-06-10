//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if REVERB_SUITE_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

#include <algorithm>

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kAlgorithmDefault = 0.0;
constexpr double kMixDefault = 0.25;
constexpr double kDecayDefault = 0.50;
constexpr double kToneDefault = 0.60;
constexpr double kPreDelayDefault = 20.0;
constexpr double kWidthDwellDefault = 0.60;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API ReverbSuiteVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* algorithm = new Vst::StringListParameter (STR16 ("Model"), kParamAlgorithm);
    algorithm->appendString (STR16 ("Room"));
    algorithm->appendString (STR16 ("Hall"));
    algorithm->appendString (STR16 ("Plate"));
    algorithm->appendString (STR16 ("Spring"));
    algorithm->appendString (STR16 ("Twin"));
    algorithm->appendString (STR16 ("Deluxe"));
    algorithm->appendString (STR16 ("Super"));
    algorithm->setNormalized (normalize (kAlgorithmDefault, 0.0, 6.0));
    parameters.addParameter (algorithm);

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mix"), kParamMix, STR16 ("%"),
                                                      0.0, 1.0, kMixDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Decay"), kParamDecay, STR16 ("%"),
                                                      0.0, 1.0, kDecayDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Tone"), kParamTone, STR16 ("%"),
                                                      0.0, 1.0, kToneDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("PreDelay"), kParamPreDelay, STR16 ("ms"),
                                                      0.0, 250.0, kPreDelayDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Width/Dwell"), kParamWidthDwell, STR16 ("%"),
                                                      0.0, 1.0, kWidthDwellDefault));
    return result;
}

tresult PLUGIN_API ReverbSuiteVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API ReverbSuiteVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float algorithm = static_cast<float> (kAlgorithmDefault);
    float mix = static_cast<float> (kMixDefault);
    float decay = static_cast<float> (kDecayDefault);
    float tone = static_cast<float> (kToneDefault);
    float preDelay = static_cast<float> (kPreDelayDefault);
    float widthDwell = static_cast<float> (kWidthDwellDefault);

    if (!streamer.readFloat (algorithm) || !streamer.readFloat (mix) || !streamer.readFloat (decay) ||
        !streamer.readFloat (tone) || !streamer.readFloat (preDelay) || !streamer.readFloat (widthDwell))
        return kResultFalse;

    setParamNormalized (kParamAlgorithm, normalize (std::clamp<double> (algorithm, 0.0, 6.0), 0.0, 6.0));
    setParamNormalized (kParamMix, normalize (std::clamp<double> (mix, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamDecay, normalize (std::clamp<double> (decay, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamTone, normalize (std::clamp<double> (tone, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamPreDelay, normalize (std::clamp<double> (preDelay, 0.0, 250.0), 0.0, 250.0));
    setParamNormalized (kParamWidthDwell, normalize (std::clamp<double> (widthDwell, 0.0, 1.0), 0.0, 1.0));
    return kResultOk;
}

tresult PLUGIN_API ReverbSuiteVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API ReverbSuiteVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API ReverbSuiteVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if REVERB_SUITE_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (Steinberg::ViewRect (0, 0, 860, 520), this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
