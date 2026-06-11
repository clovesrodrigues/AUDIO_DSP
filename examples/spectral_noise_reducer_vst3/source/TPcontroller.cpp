//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPparameters.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#if SPECTRAL_NOISE_REDUCER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace Params = SpectralNoiseReducerParameters;
namespace {
void addParameter (
    Vst::ParameterContainer& parameters,
    const Vst::TChar* title,
    const Vst::TChar* units,
    int32 stepCount,
    Vst::ParamValue defaultValue,
    int32 flags,
    Vst::ParamID id)
{
    parameters.addParameter (title, units, stepCount, defaultValue, flags, id);
}
} // namespace

tresult PLUGIN_API SpectralNoiseReducerVST3Controller::initialize (FUnknown* context)
{
    const tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    addParameter (parameters, STR16 ("Perceber Ruido"), STR16 (""), 1, Params::kDefaultValues[0], Vst::ParameterInfo::kCanAutomate, Params::kLearnNoise);
    addParameter (parameters, STR16 ("Subtrair Ruido"), STR16 (""), 1, Params::kDefaultValues[1], Vst::ParameterInfo::kCanAutomate, Params::kSubtractNoise);
    addParameter (parameters, STR16 ("Ganho"), STR16 ("dB"), 0, Params::kDefaultValues[2], Vst::ParameterInfo::kCanAutomate, Params::kOutputGain);
    addParameter (parameters, STR16 ("Presenca"), STR16 ("%"), 0, Params::kDefaultValues[3], Vst::ParameterInfo::kCanAutomate, Params::kPresenceProtect);
    addParameter (parameters, STR16 ("Smooth"), STR16 ("%"), 0, Params::kDefaultValues[4], Vst::ParameterInfo::kCanAutomate, Params::kSmoothing);

    return result;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API SpectralNoiseReducerVST3Controller::setComponentState (IBStream* state)
{
    if (state == nullptr)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 index = 0; index < Params::kParameterCount; ++index)
    {
        float normalized = static_cast<float> (Params::kDefaultValues[index]);
        if (!streamer.readFloat (normalized))
            return kResultFalse;
        setParamNormalized (Params::kParameterIDs[index], normalized);
    }
    return kResultOk;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API SpectralNoiseReducerVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if SPECTRAL_NOISE_REDUCER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (Steinberg::ViewRect (0, 0, 980, 620), this, "CV Spectral Noise Reducer");
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
