//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if TAPE_SAT_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kDriveMin = 1.0;
constexpr double kDriveMax = 30.0;
constexpr double kDriveDefault = 3.0;
constexpr double kBiasDefault = 0.0;
constexpr double kMixDefault = 1.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

tresult PLUGIN_API TapeSatVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Drive"), kParamTapeSatDrive, STR16 (""),
                                                      kDriveMin, kDriveMax, kDriveDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bias"), kParamTapeSatBias, STR16 (""),
                                                      -1.0, 1.0, kBiasDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mix"), kParamTapeSatMix, STR16 (""),
                                                      0.0, 1.0, kMixDefault));
    return result;
}

tresult PLUGIN_API TapeSatVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API TapeSatVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float drive = static_cast<float> (kDriveDefault);
    float bias = static_cast<float> (kBiasDefault);
    float mix = static_cast<float> (kMixDefault);

    if (!streamer.readFloat (drive) || !streamer.readFloat (bias) || !streamer.readFloat (mix))
        return kResultFalse;

    setParamNormalized (kParamTapeSatDrive, normalize (drive, kDriveMin, kDriveMax));
    setParamNormalized (kParamTapeSatBias, normalize (bias, -1.0, 1.0));
    setParamNormalized (kParamTapeSatMix, normalize (mix, 0.0, 1.0));
    return kResultOk;
}

tresult PLUGIN_API TapeSatVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API TapeSatVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API TapeSatVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if TAPE_SAT_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
