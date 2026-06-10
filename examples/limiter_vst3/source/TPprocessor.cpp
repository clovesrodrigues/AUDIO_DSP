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
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;
}

LimiterVST3Processor::LimiterVST3Processor ()
{
    setControllerClass (kLimiterVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

LimiterVST3Processor::~LimiterVST3Processor ()
{}

tresult PLUGIN_API LimiterVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API LimiterVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API LimiterVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& limiter : limiters_)
            limiter.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API LimiterVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : sampleRate_;
    for (auto& limiter : limiters_)
        limiter.prepare (static_cast<float> (sampleRate_));

    cvdsp::manager::ParameterSmoothingConfig<float> smoothing {};
    smoothing.sampleRate = static_cast<float> (sampleRate_);
    smoothing.rampTimeSeconds = 0.010f;
    (void)parameters_.prepare (static_cast<float> (sampleRate_), static_cast<std::size_t> (newSetup.maxSamplesPerBlock), smoothing);
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API LimiterVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API LimiterVST3Processor::process (Vst::ProcessData& data)
{
    const auto sampleCount = static_cast<std::size_t> (std::max<int32> (data.numSamples, 0));
    parameters_.beginBlock (sampleCount);
    cvdsp::adapters::vst3::VST3ParameterAdapter::adaptParameterChanges (data.inputParameterChanges, parameters_);
    parameters_.processBlockParameters (sampleCount);
    applyParametersToDSP ();

    if (data.numSamples <= 0)
        return kResultOk;

    const float outputGain = dbToLinear (outputGainDB_);
    const int32 minBus = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < minBus; ++bus)
    {
        const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (limiters_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& limiter = limiters_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = limiter.process (input[sample]) * outputGain;
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

tresult PLUGIN_API LimiterVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float threshold = -0.3f;
    float release = 50.0f;
    float outputGain = 0.0f;
    if (!streamer.readFloat (threshold) || !streamer.readFloat (release) || !streamer.readFloat (outputGain))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamLimiterThreshold, threshold);
    (void)parameters_.setImmediateReal (kParamLimiterRelease, release);
    (void)parameters_.setImmediateReal (kParamLimiterOutputGain, outputGain);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API LimiterVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (thresholdDB_) || !streamer.writeFloat (releaseMs_) || !streamer.writeFloat (outputGainDB_))
        return kResultFalse;
    return kResultOk;
}

void LimiterVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamLimiterThreshold, "Thresh", "Threshold",
        ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
        {-24.0f, 0.0f, -0.3f, 0.0f, 1.0f}, nullptr, 0, "threshold", "dBFS", "Limiter", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamLimiterRelease, "Release", "Release",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {1.0f, 1000.0f, 50.0f, 0.0f, 1.0f}, nullptr, 0, "release", "ms", "Limiter", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamLimiterOutputGain, "Output", "Output Gain",
        ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
        {-12.0f, 12.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "output_gain", "dB", "Limiter", 2),
        ParameterSmoothingMode::Linear);
}

void LimiterVST3Processor::applyParametersToDSP () noexcept
{
    thresholdDB_ = parameters_.getCurrentReal (kParamLimiterThreshold);
    releaseMs_ = parameters_.getCurrentReal (kParamLimiterRelease);
    outputGainDB_ = parameters_.getCurrentReal (kParamLimiterOutputGain);

    for (auto& limiter : limiters_)
    {
        limiter.setThresholdDB (thresholdDB_);
        limiter.setReleaseMs (releaseMs_);
    }
}

float LimiterVST3Processor::dbToLinear (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

} // namespace CV
