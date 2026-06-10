//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if TUBE_PREAMP_VST3_ENABLE_CV_GUI
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

tresult PLUGIN_API TubePreampVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Drive"), kParamTubePreampDrive, STR16 (""),
                                                      0.0, 30.0, 4.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bias"), kParamTubePreampBias, STR16 (""),
                                                      -1.0, 1.0, 0.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Plate Voltage"), kParamTubePreampPlateVoltage, STR16 ("V"),
                                                      80.0, 400.0, 250.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Stages"), kParamTubePreampStages, STR16 (""),
                                                      1.0, 4.0, 2.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Output Gain"), kParamTubePreampOutputGain, STR16 (""),
                                                      0.0, 4.0, 1.0));
    return result;
}

tresult PLUGIN_API TubePreampVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API TubePreampVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float drive = 4.0f;
    float bias = 0.0f;
    float plateVoltage = 250.0f;
    float stages = 2.0f;
    float outputGain = 1.0f;

    if (!streamer.readFloat (drive) || !streamer.readFloat (bias) || !streamer.readFloat (plateVoltage) || !streamer.readFloat (stages) || !streamer.readFloat (outputGain))
        return kResultFalse;

    setParamNormalized (kParamTubePreampDrive, normalize (drive, 0.0, 30.0));
    setParamNormalized (kParamTubePreampBias, normalize (bias, -1.0, 1.0));
    setParamNormalized (kParamTubePreampPlateVoltage, normalize (plateVoltage, 80.0, 400.0));
    setParamNormalized (kParamTubePreampStages, normalize (stages, 1.0, 4.0));
    setParamNormalized (kParamTubePreampOutputGain, normalize (outputGain, 0.0, 4.0));
    return kResultOk;
}

tresult PLUGIN_API TubePreampVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API TubePreampVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API TubePreampVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if TUBE_PREAMP_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
