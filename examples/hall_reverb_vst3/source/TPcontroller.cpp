//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if HALL_REVERB_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

#include <algorithm>

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kMixDefault = 0.25;
constexpr double kDecayDefault = 0.50;
constexpr double kDampingDefault = 0.35;
constexpr double kPreDelayDefault = 20.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API HallReverbVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mix"), kParamMix, STR16 ("%"),
                                                      0.0, 1.0, kMixDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Decay"), kParamDecay, STR16 ("%"),
                                                      0.0, 1.0, kDecayDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Damping"), kParamDamping, STR16 ("%"),
                                                      0.0, 1.0, kDampingDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("PreDelay"), kParamPreDelay, STR16 ("ms"),
                                                      0.0, 250.0, kPreDelayDefault));
    return result;
}

tresult PLUGIN_API HallReverbVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API HallReverbVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mix = static_cast<float> (kMixDefault);
    float decay = static_cast<float> (kDecayDefault);
    float damping = static_cast<float> (kDampingDefault);
    float preDelay = static_cast<float> (kPreDelayDefault);

    if (!streamer.readFloat (mix) || !streamer.readFloat (decay) || !streamer.readFloat (damping) ||
        !streamer.readFloat (preDelay))
        return kResultFalse;

    setParamNormalized (kParamMix, normalize (std::clamp<double> (mix, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamDecay, normalize (std::clamp<double> (decay, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamDamping, normalize (std::clamp<double> (damping, 0.0, 1.0), 0.0, 1.0));
    setParamNormalized (kParamPreDelay, normalize (std::clamp<double> (preDelay, 0.0, 250.0), 0.0, 250.0));
    return kResultOk;
}

tresult PLUGIN_API HallReverbVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API HallReverbVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API HallReverbVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if HALL_REVERB_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (Steinberg::ViewRect (0, 0, 760, 420), this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
