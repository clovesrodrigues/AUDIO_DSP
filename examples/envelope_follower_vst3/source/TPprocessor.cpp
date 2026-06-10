//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace CV {
namespace {
constexpr cvdsp::manager::ParameterEnumEntry kModeEntries[] {{kEnvelopeModePeak, "Peak"}, {kEnvelopeModeRMS, "RMS"}};
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;
}

EnvelopeFollowerVST3Processor::EnvelopeFollowerVST3Processor ()
{
    setControllerClass (kEnvelopeFollowerVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

EnvelopeFollowerVST3Processor::~EnvelopeFollowerVST3Processor ()
{}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& follower : followers_)
            follower.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : sampleRate_;
    for (auto& follower : followers_)
        follower.prepare (static_cast<float> (sampleRate_));

    cvdsp::manager::ParameterSmoothingConfig<float> smoothing {};
    smoothing.sampleRate = static_cast<float> (sampleRate_);
    smoothing.rampTimeSeconds = 0.010f;
    (void)parameters_.prepare (static_cast<float> (sampleRate_), static_cast<std::size_t> (newSetup.maxSamplesPerBlock), smoothing);
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::process (Vst::ProcessData& data)
{
    const auto sampleCount = static_cast<std::size_t> (std::max<int32> (data.numSamples, 0));
    parameters_.beginBlock (sampleCount);
    cvdsp::adapters::vst3::VST3ParameterAdapter::adaptParameterChanges (data.inputParameterChanges, parameters_);
    parameters_.processBlockParameters (sampleCount);
    applyParametersToDSP ();

    if (data.numSamples <= 0)
        return kResultOk;

    const float wet = std::clamp (mixPercent_ * 0.01f, 0.0f, 1.0f);
    const float dry = 1.0f - wet;
    const float outputGain = dbToLinear (outputGainDB_);

    const int32 minBus = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < minBus; ++bus)
    {
        const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (followers_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& follower = followers_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
            {
                const float x = input[sample];
                const float envelope = follower.process (x) * outputGain;
                output[sample] = dry * x + wet * envelope;
            }
        }

        for (int32 channel = processChan; channel < minChan; ++channel)
        {
            if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
            {
                std::memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel],
                             sampleCount * sizeof (Vst::Sample32));
            }
        }
        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }
    return kResultOk;
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    int32 mode = kEnvelopeModePeak;
    float attack = 10.0f;
    float release = 100.0f;
    float outputGain = 0.0f;
    float mix = 100.0f;
    if (!streamer.readInt32 (mode) || !streamer.readFloat (attack) || !streamer.readFloat (release) ||
        !streamer.readFloat (outputGain) || !streamer.readFloat (mix))
        return kResultFalse;

    (void)parameters_.setImmediateNormalized (kParamEnvelopeMode, static_cast<float> (modeIndexToNormalized (mode)));
    (void)parameters_.setImmediateReal (kParamEnvelopeAttack, attack);
    (void)parameters_.setImmediateReal (kParamEnvelopeRelease, release);
    (void)parameters_.setImmediateReal (kParamEnvelopeOutputGain, outputGain);
    (void)parameters_.setImmediateReal (kParamEnvelopeMix, mix);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API EnvelopeFollowerVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    const int32 mode = normalizedToModeIndex (parameters_.getTargetNormalized (kParamEnvelopeMode));
    if (!streamer.writeInt32 (mode) || !streamer.writeFloat (attackMs_) || !streamer.writeFloat (releaseMs_) ||
        !streamer.writeFloat (outputGainDB_) || !streamer.writeFloat (mixPercent_))
        return kResultFalse;
    return kResultOk;
}

void EnvelopeFollowerVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamEnvelopeMode, "Mode", "Mode",
        ParameterUnit::Index, ParameterScale::Enum, kParamFlags,
        {0.0f, 1.0f, 0.0f, 1.0f, 1.0f}, kModeEntries, kEnvelopeModeCount, "mode", "", "Envelope Follower", 0),
        ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamEnvelopeAttack, "Attack", "Attack",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {0.1f, 200.0f, 10.0f, 0.0f, 1.0f}, nullptr, 0, "attack", "ms", "Envelope Follower", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamEnvelopeRelease, "Release", "Release",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {1.0f, 2000.0f, 100.0f, 0.0f, 1.0f}, nullptr, 0, "release", "ms", "Envelope Follower", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamEnvelopeOutputGain, "Output", "Output Gain",
        ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
        {-24.0f, 24.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "output_gain", "dB", "Envelope Follower", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamEnvelopeMix, "Mix", "Wet Mix",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 100.0f, 100.0f, 0.0f, 1.0f}, nullptr, 0, "mix", "%", "Envelope Follower", 1),
        ParameterSmoothingMode::Linear);
}

void EnvelopeFollowerVST3Processor::applyParametersToDSP () noexcept
{
    modeIndex_ = normalizedToModeIndex (parameters_.getTargetNormalized (kParamEnvelopeMode));
    attackMs_ = parameters_.getCurrentReal (kParamEnvelopeAttack);
    releaseMs_ = parameters_.getCurrentReal (kParamEnvelopeRelease);
    outputGainDB_ = parameters_.getCurrentReal (kParamEnvelopeOutputGain);
    mixPercent_ = parameters_.getCurrentReal (kParamEnvelopeMix);

    const auto mode = modeIndex_ == kEnvelopeModeRMS ? cvdsp::dynamics::EnvelopeMode::RMS
                                                     : cvdsp::dynamics::EnvelopeMode::Peak;
    for (auto& follower : followers_)
    {
        follower.setMode (mode);
        follower.setAttackMs (attackMs_);
        follower.setReleaseMs (releaseMs_);
    }
}

int32 EnvelopeFollowerVST3Processor::normalizedToModeIndex (float normalized) noexcept
{
    return std::clamp<int32> (static_cast<int32> (std::clamp (normalized, 0.0f, 1.0f) + 0.5f), 0, 1);
}

Vst::ParamValue EnvelopeFollowerVST3Processor::modeIndexToNormalized (int32 modeIndex) noexcept
{
    return modeIndex == kEnvelopeModeRMS ? 1.0 : 0.0;
}

float EnvelopeFollowerVST3Processor::dbToLinear (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

} // namespace CV
