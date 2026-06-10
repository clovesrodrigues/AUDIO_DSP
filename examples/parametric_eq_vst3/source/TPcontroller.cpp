//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if PARAMETRIC_EQ_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kFrequencyDefaults[5] {80.0, 250.0, 1000.0, 4000.0, 12000.0};
constexpr double kQDefault = 0.707;
constexpr double kGainDefault = 0.0;
constexpr int32 kTypeDefaults[5] {kParametricEQLowShelf, kParametricEQPeaking, kParametricEQPeaking,
                                  kParametricEQPeaking, kParametricEQHighShelf};

constexpr const Vst::TChar* kTypeNames[5] {STR16 ("B1 Type"), STR16 ("B2 Type"), STR16 ("B3 Type"), STR16 ("B4 Type"), STR16 ("B5 Type")};
constexpr const Vst::TChar* kFrequencyNames[5] {STR16 ("B1 Freq"), STR16 ("B2 Freq"), STR16 ("B3 Freq"), STR16 ("B4 Freq"), STR16 ("B5 Freq")};
constexpr const Vst::TChar* kQNames[5] {STR16 ("B1 Q"), STR16 ("B2 Q"), STR16 ("B3 Q"), STR16 ("B4 Q"), STR16 ("B5 Q")};
constexpr const Vst::TChar* kGainNames[5] {STR16 ("B1 Gain"), STR16 ("B2 Gain"), STR16 ("B3 Gain"), STR16 ("B4 Gain"), STR16 ("B5 Gain")};

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}

void appendFilterTypeStrings (Vst::StringListParameter& parameter)
{
    parameter.appendString (STR16 ("LowPass"));
    parameter.appendString (STR16 ("HighPass"));
    parameter.appendString (STR16 ("Peaking"));
    parameter.appendString (STR16 ("LowShelf"));
    parameter.appendString (STR16 ("HighShelf"));
    parameter.appendString (STR16 ("Notch"));
}
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    for (int32 band = 0; band < static_cast<int32> (kParametricEQBandCount); ++band)
    {
        auto* type = new Vst::StringListParameter (kTypeNames[band], parametricEQParamID (band, kParametricEQTypeOffset));
        appendFilterTypeStrings (*type);
        type->setNormalized (static_cast<Vst::ParamValue> (kTypeDefaults[band]) /
                             static_cast<Vst::ParamValue> (kParametricEQFilterTypeCount - 1));
        parameters.addParameter (type);

        parameters.addParameter (new Vst::RangeParameter (kFrequencyNames[band], parametricEQParamID (band, kParametricEQFrequencyOffset),
                                                          STR16 ("Hz"), 20.0, 20000.0, kFrequencyDefaults[band]));

        parameters.addParameter (new Vst::RangeParameter (kQNames[band], parametricEQParamID (band, kParametricEQQOffset),
                                                          STR16 (""), 0.10, 10.0, kQDefault));

        parameters.addParameter (new Vst::RangeParameter (kGainNames[band], parametricEQParamID (band, kParametricEQGainOffset),
                                                          STR16 ("dB"), -24.0, 24.0, kGainDefault));
    }

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 band = 0; band < static_cast<int32> (kParametricEQBandCount); ++band)
    {
        int32 type = kTypeDefaults[band];
        float frequency = static_cast<float> (kFrequencyDefaults[band]);
        float q = static_cast<float> (kQDefault);
        float gain = static_cast<float> (kGainDefault);

        if (!streamer.readInt32 (type) || !streamer.readFloat (frequency) ||
            !streamer.readFloat (q) || !streamer.readFloat (gain))
        {
            return kResultFalse;
        }

        setParamNormalized (parametricEQParamID (band, kParametricEQTypeOffset),
                            static_cast<Vst::ParamValue> (type) / static_cast<Vst::ParamValue> (kParametricEQFilterTypeCount - 1));
        setParamNormalized (parametricEQParamID (band, kParametricEQFrequencyOffset), normalize (frequency, 20.0, 20000.0));
        setParamNormalized (parametricEQParamID (band, kParametricEQQOffset), normalize (q, 0.10, 10.0));
        setParamNormalized (parametricEQParamID (band, kParametricEQGainOffset), normalize (gain, -24.0, 24.0));
    }

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API ParametricEQVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if PARAMETRIC_EQ_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
