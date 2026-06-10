//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if LADDER_FILTER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kCutoffDefault = 1000.0;
constexpr double kResonanceDefault = 0.0;
constexpr double kDriveDefault = 1.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API LadderFilterVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Cutoff"), kParamLadderCutoff, STR16 ("Hz"), 20.0, 20000.0, kCutoffDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Resonance"), kParamLadderResonance, STR16 (""), 0.0, 4.0, kResonanceDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Drive"), kParamLadderDrive, STR16 ("x"), 1.0, 20.0, kDriveDefault));
    return result;
}

tresult PLUGIN_API LadderFilterVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API LadderFilterVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float cutoff = static_cast<float> (kCutoffDefault);
    float resonance = static_cast<float> (kResonanceDefault);
    float drive = static_cast<float> (kDriveDefault);

    if (!streamer.readFloat (cutoff) || !streamer.readFloat (resonance) || !streamer.readFloat (drive))
        return kResultFalse;

    setParamNormalized (kParamLadderCutoff, normalize (cutoff, 20.0, 20000.0));
    setParamNormalized (kParamLadderResonance, normalize (resonance, 0.0, 4.0));
    setParamNormalized (kParamLadderDrive, normalize (drive, 1.0, 20.0));
    return kResultOk;
}

tresult PLUGIN_API LadderFilterVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API LadderFilterVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API LadderFilterVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if LADDER_FILTER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
