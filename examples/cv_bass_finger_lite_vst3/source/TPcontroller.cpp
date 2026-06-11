//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kToneDefault = 0.38;
constexpr double kAttackDefault = 3.0;
constexpr double kVelocitySensitivityDefault = 0.75;
constexpr double kOutputGainDefault = -6.0;
constexpr double kCompressionDefault = 0.15;
constexpr double kDriveDefault = 0.0;
constexpr double kBassEQDefault = 0.0;
constexpr double kMidEQDefault = 0.0;
constexpr double kTrebleEQDefault = 0.0;
constexpr double kFingerNoiseDefault = 0.0;
constexpr double kHumanizeDefault = 0.04;
constexpr double kRoomDefault = 0.0;

Vst::ParamValue normalize (double plain, double minPlain, double maxPlain)
{
    return (plain - minPlain) / (maxPlain - minPlain);
}
} // namespace

tresult PLUGIN_API CVBassFingerLiteController::initialize (FUnknown* context)
{
    const tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Tone"), kParamBassTone, STR16 ("%"), 0.0, 1.0, kToneDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Attack"), kParamBassAttack, STR16 ("ms"), 0.5, 40.0, kAttackDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Velocity Sensitivity"), kParamBassVelocitySensitivity, STR16 ("%"), 0.0, 1.0, kVelocitySensitivityDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Output Gain"), kParamBassOutputGain, STR16 ("dB"), -24.0, 6.0, kOutputGainDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Compression"), kParamBassCompression, STR16 ("%"), 0.0, 1.0, kCompressionDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Drive"), kParamBassDrive, STR16 ("%"), 0.0, 1.0, kDriveDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bass"), kParamBassLowEQ, STR16 ("dB"), -12.0, 12.0, kBassEQDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mid"), kParamBassMidEQ, STR16 ("dB"), -12.0, 12.0, kMidEQDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Treble"), kParamBassHighEQ, STR16 ("dB"), -12.0, 12.0, kTrebleEQDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Finger Noise"), kParamBassFingerNoise, STR16 ("%"), 0.0, 1.0, kFingerNoiseDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Humanize"), kParamBassHumanize, STR16 ("%"), 0.0, 1.0, kHumanizeDefault));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Room"), kParamBassRoom, STR16 ("%"), 0.0, 1.0, kRoomDefault));

    return result;
}

tresult PLUGIN_API CVBassFingerLiteController::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API CVBassFingerLiteController::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float tone = static_cast<float> (kToneDefault);
    float attack = static_cast<float> (kAttackDefault);
    float velocitySensitivity = static_cast<float> (kVelocitySensitivityDefault);
    float outputGain = static_cast<float> (kOutputGainDefault);
    float compression = static_cast<float> (kCompressionDefault);
    float drive = static_cast<float> (kDriveDefault);
    float bassGain = static_cast<float> (kBassEQDefault);
    float midGain = static_cast<float> (kMidEQDefault);
    float trebleGain = static_cast<float> (kTrebleEQDefault);
    float fingerNoise = static_cast<float> (kFingerNoiseDefault);
    float humanize = static_cast<float> (kHumanizeDefault);
    float roomMix = static_cast<float> (kRoomDefault);
    if (!streamer.readFloat (tone) || !streamer.readFloat (attack) ||
        !streamer.readFloat (velocitySensitivity) || !streamer.readFloat (outputGain))
        return kResultFalse;

    (void)streamer.readFloat (compression);
    (void)streamer.readFloat (drive);
    (void)streamer.readFloat (bassGain);
    (void)streamer.readFloat (midGain);
    (void)streamer.readFloat (trebleGain);
    (void)streamer.readFloat (fingerNoise);
    (void)streamer.readFloat (humanize);
    (void)streamer.readFloat (roomMix);

    setParamNormalized (kParamBassTone, normalize (tone, 0.0, 1.0));
    setParamNormalized (kParamBassAttack, normalize (attack, 0.5, 40.0));
    setParamNormalized (kParamBassVelocitySensitivity, normalize (velocitySensitivity, 0.0, 1.0));
    setParamNormalized (kParamBassOutputGain, normalize (outputGain, -24.0, 6.0));
    setParamNormalized (kParamBassCompression, normalize (compression, 0.0, 1.0));
    setParamNormalized (kParamBassDrive, normalize (drive, 0.0, 1.0));
    setParamNormalized (kParamBassLowEQ, normalize (bassGain, -12.0, 12.0));
    setParamNormalized (kParamBassMidEQ, normalize (midGain, -12.0, 12.0));
    setParamNormalized (kParamBassHighEQ, normalize (trebleGain, -12.0, 12.0));
    setParamNormalized (kParamBassFingerNoise, normalize (fingerNoise, 0.0, 1.0));
    setParamNormalized (kParamBassHumanize, normalize (humanize, 0.0, 1.0));
    setParamNormalized (kParamBassRoom, normalize (roomMix, 0.0, 1.0));
    return kResultOk;
}

tresult PLUGIN_API CVBassFingerLiteController::setState (IBStream* state)
{
    return state ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API CVBassFingerLiteController::getState (IBStream* state)
{
    return state ? kResultTrue : kResultFalse;
}

IPlugView* PLUGIN_API CVBassFingerLiteController::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

    return nullptr;
}

} // namespace CV
