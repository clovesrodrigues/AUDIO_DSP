//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if GRAPHIC_EQ_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kGainDefault = 0.0;
constexpr const Vst::TChar* kGainNames[10] {STR16 ("31 Hz"), STR16 ("63 Hz"), STR16 ("125 Hz"),
                                            STR16 ("250 Hz"), STR16 ("500 Hz"), STR16 ("1 kHz"),
                                            STR16 ("2 kHz"), STR16 ("4 kHz"), STR16 ("8 kHz"),
                                            STR16 ("16 kHz")};

Vst::ParamValue normalizeGain (double gainDB)
{
    return (gainDB + 24.0) / 48.0;
}
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    for (int32 band = 0; band < static_cast<int32> (kGraphicEQBandCount); ++band)
    {
        parameters.addParameter (new Vst::RangeParameter (kGainNames[band], graphicEQGainParamID (band),
                                                          STR16 ("dB"), -24.0, 24.0, kGainDefault));
    }

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 band = 0; band < static_cast<int32> (kGraphicEQBandCount); ++band)
    {
        float gain = static_cast<float> (kGainDefault);
        if (!streamer.readFloat (gain))
            return kResultFalse;
        setParamNormalized (graphicEQGainParamID (band), normalizeGain (gain));
    }

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API GraphicEQVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if GRAPHIC_EQ_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
