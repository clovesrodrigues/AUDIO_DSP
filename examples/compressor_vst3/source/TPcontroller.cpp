//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if COMPRESSOR_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kThresholdDefault = -18.0;
constexpr double kRatioDefault = 4.0;
constexpr double kAttackDefault = 10.0;
constexpr double kReleaseDefault = 100.0;
constexpr double kKneeDefault = 6.0;
constexpr double kMakeupDefault = 3.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
}

//------------------------------------------------------------------------
// CompressorVST3Controller Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Threshold"), kParamThreshold, STR16 ("dB"),
                                                      -60.0, 0.0, kThresholdDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Ratio"), kParamRatio, STR16 (":1"),
                                                      1.0, 20.0, kRatioDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Attack"), kParamAttack, STR16 ("ms"),
                                                      0.1, 200.0, kAttackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Release"), kParamRelease, STR16 ("ms"),
                                                      10.0, 1000.0, kReleaseDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Knee"), kParamKnee, STR16 ("dB"),
                                                      0.0, 24.0, kKneeDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Makeup"), kParamMakeup, STR16 ("dB"),
                                                      0.0, 24.0, kMakeupDefault));

    auto* detector = new Vst::StringListParameter (STR16 ("Detector"), kParamDetector);
    detector->appendString (STR16 ("Peak"));
    detector->appendString (STR16 ("RMS"));
    detector->setNormalized (0.0);
    parameters.addParameter (detector);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float threshold = static_cast<float> (kThresholdDefault);
    float ratio = static_cast<float> (kRatioDefault);
    float attack = static_cast<float> (kAttackDefault);
    float release = static_cast<float> (kReleaseDefault);
    float knee = static_cast<float> (kKneeDefault);
    float makeup = static_cast<float> (kMakeupDefault);
    int32 detector = 0;

    if (!streamer.readFloat (threshold) ||
        !streamer.readFloat (ratio) ||
        !streamer.readFloat (attack) ||
        !streamer.readFloat (release) ||
        !streamer.readFloat (knee) ||
        !streamer.readFloat (makeup) ||
        !streamer.readInt32 (detector))
    {
        return kResultFalse;
    }

    setParamNormalized (kParamThreshold, normalize (threshold, -60.0, 0.0));
    setParamNormalized (kParamRatio, normalize (ratio, 1.0, 20.0));
    setParamNormalized (kParamAttack, normalize (attack, 0.1, 200.0));
    setParamNormalized (kParamRelease, normalize (release, 10.0, 1000.0));
    setParamNormalized (kParamKnee, normalize (knee, 0.0, 24.0));
    setParamNormalized (kParamMakeup, normalize (makeup, 0.0, 24.0));
    setParamNormalized (kParamDetector, detector == 1 ? 1.0 : 0.0);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API CompressorVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if COMPRESSOR_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    // Fallback: returning nullptr intentionally exposes the native VST3
    // parameter editor supplied by hosts/DAWs when CV_GUI is unavailable for
    // this platform or disabled by the build configuration.
    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
