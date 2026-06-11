//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"
#include "TPparameters.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace CV {
namespace Params = SpectralNoiseReducerParameters;

SpectralNoiseReducerVST3Processor::SpectralNoiseReducerVST3Processor ()
{
    setControllerClass (kSpectralNoiseReducerVST3ControllerUID);
    resetParametersToDefaults ();
    applyAllParametersToDSP ();
}

SpectralNoiseReducerVST3Processor::~SpectralNoiseReducerVST3Processor ()
{}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::initialize (FUnknown* context)
{
    const tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::setActive (TBool state)
{
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& processor : processors_)
        (void)processor.prepare (static_cast<float> (sampleRate_));

    applyAllParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::process (Vst::ProcessData& data)
{
    if (data.symbolicSampleSize != Vst::kSample32)
        return kResultFalse;

    if (data.inputParameterChanges != nullptr)
    {
        const int32 parameterQueueCount = data.inputParameterChanges->getParameterCount ();
        for (int32 queueIndex = 0; queueIndex < parameterQueueCount; ++queueIndex)
        {
            Vst::IParamValueQueue* queue = data.inputParameterChanges->getParameterData (queueIndex);
            if (queue == nullptr || queue->getPointCount () <= 0)
                continue;

            int32 sampleOffset = 0;
            Vst::ParamValue normalizedValue = 0.0;
            if (queue->getPoint (queue->getPointCount () - 1, sampleOffset, normalizedValue) != kResultOk)
                continue;

            (void)sampleOffset;
            applyParameterToDSP (queue->getParameterId (), normalizedValue);
        }
    }

    if (data.numSamples <= 0)
        return kResultOk;

    const int32 minBus = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < minBus; ++bus)
    {
        const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (processors_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            if (input == nullptr || output == nullptr)
                continue;

            if (bypassed_)
            {
                if (output != input)
                    std::memcpy (output, input, static_cast<std::size_t> (data.numSamples) * sizeof (Vst::Sample32));
                continue;
            }

            auto& processor = processors_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = processor.processSample (input[sample]);
        }

        for (int32 channel = processChan; channel < minChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            if (input != nullptr && output != nullptr && output != input)
                std::memcpy (output, input, static_cast<std::size_t> (data.numSamples) * sizeof (Vst::Sample32));
        }

        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }

    clearRemainingOutputs (data, minBus);
    return kResultOk;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::setState (IBStream* state)
{
    if (state == nullptr)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 index = 0; index < Params::kParameterCount; ++index)
    {
        float normalized = static_cast<float> (Params::kDefaultValues[index]);
        if (!streamer.readFloat (normalized))
            return kResultFalse;
        applyParameterToDSP (Params::kParameterIDs[index], normalized);
    }
    return kResultOk;
}

tresult PLUGIN_API SpectralNoiseReducerVST3Processor::getState (IBStream* state)
{
    if (state == nullptr)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 index = 0; index < Params::kParameterCount; ++index)
    {
        const float normalized = static_cast<float> (getParameterNormalized (Params::kParameterIDs[index]));
        streamer.writeFloat (normalized);
    }
    return kResultOk;
}

float SpectralNoiseReducerVST3Processor::clamp01 (double value) noexcept
{
    return static_cast<float> (std::clamp (value, 0.0, 1.0));
}

float SpectralNoiseReducerVST3Processor::normalizedToOutputGainDb (double normalized) noexcept
{
    return -24.0f + clamp01 (normalized) * 48.0f;
}

float SpectralNoiseReducerVST3Processor::normalizedToSpectralFloorDb (double normalized) noexcept
{
    return -120.0f + clamp01 (normalized) * 108.0f;
}

float SpectralNoiseReducerVST3Processor::normalizedToMaxReductionDb (double normalized) noexcept
{
    return clamp01 (normalized) * 80.0f;
}

std::size_t SpectralNoiseReducerVST3Processor::normalizedToFrequencySmoothingBins (double normalized) noexcept
{
    return static_cast<std::size_t> (std::lround (clamp01 (normalized) * 12.0f));
}

void SpectralNoiseReducerVST3Processor::resetParametersToDefaults () noexcept
{
    for (int32 index = 0; index < Params::kParameterCount; ++index)
        parameters_[static_cast<std::size_t> (index)] = Params::kDefaultValues[index];
}

void SpectralNoiseReducerVST3Processor::applyAllParametersToDSP () noexcept
{
    for (int32 index = 0; index < Params::kParameterCount; ++index)
        applyParameterToDSP (Params::kParameterIDs[index], parameters_[static_cast<std::size_t> (index)]);
}

void SpectralNoiseReducerVST3Processor::applyParameterToDSP (
    Vst::ParamID id,
    Vst::ParamValue normalizedValue) noexcept
{
    normalizedValue = clamp01 (normalizedValue);
    setParameterNormalized (id, normalizedValue);

    switch (id)
    {
        case Params::kBypass:
            bypassed_ = normalizedValue >= 0.5;
            break;
        case Params::kLearnNoise:
            for (auto& processor : processors_)
                processor.setLearnNoiseEnabled (normalizedValue >= 0.5);
            break;
        case Params::kSubtractNoise:
            for (auto& processor : processors_)
                processor.setSubtractNoiseEnabled (normalizedValue >= 0.5);
            break;
        case Params::kClearProfile:
            if (normalizedValue >= 0.5)
            {
                for (auto& processor : processors_)
                    processor.triggerClearProfile ();
            }
            break;
        case Params::kOutputGain:
            for (auto& processor : processors_)
                processor.setOutputGainDb (normalizedToOutputGainDb (normalizedValue));
            break;
        case Params::kPresenceProtect:
            for (auto& processor : processors_)
                processor.setPresenceProtect (clamp01 (normalizedValue));
            break;
        case Params::kReductionAmount:
            for (auto& processor : processors_)
                processor.setReductionAmount (clamp01 (normalizedValue));
            break;
        case Params::kSpectralFloor:
            for (auto& processor : processors_)
                processor.setSpectralFloorDb (normalizedToSpectralFloorDb (normalizedValue));
            break;
        case Params::kMaxReduction:
            for (auto& processor : processors_)
                processor.setMaxReductionDb (normalizedToMaxReductionDb (normalizedValue));
            break;
        case Params::kSmoothing:
            for (auto& processor : processors_)
                processor.setSmoothing (clamp01 (normalizedValue));
            break;
        case Params::kFrequencySmoothing:
            for (auto& processor : processors_)
                processor.setFrequencySmoothingBins (normalizedToFrequencySmoothingBins (normalizedValue));
            break;
        case Params::kTransientProtection:
            for (auto& processor : processors_)
                processor.setTransientProtection (clamp01 (normalizedValue));
            break;
        case Params::kMix:
            for (auto& processor : processors_)
                processor.setMix (clamp01 (normalizedValue));
            break;
        default:
            break;
    }
}

Vst::ParamValue SpectralNoiseReducerVST3Processor::getParameterNormalized (Vst::ParamID id) const noexcept
{
    for (int32 index = 0; index < Params::kParameterCount; ++index)
    {
        if (Params::kParameterIDs[index] == id)
            return parameters_[static_cast<std::size_t> (index)];
    }
    return 0.0;
}

void SpectralNoiseReducerVST3Processor::setParameterNormalized (
    Vst::ParamID id,
    Vst::ParamValue normalizedValue) noexcept
{
    for (int32 index = 0; index < Params::kParameterCount; ++index)
    {
        if (Params::kParameterIDs[index] == id)
        {
            parameters_[static_cast<std::size_t> (index)] = clamp01 (normalizedValue);
            return;
        }
    }
}

void SpectralNoiseReducerVST3Processor::clearRemainingOutputs (Vst::ProcessData& data, int32 firstBus) noexcept
{
    for (int32 bus = firstBus; bus < data.numOutputs; ++bus)
    {
        for (int32 channel = 0; channel < data.outputs[bus].numChannels; ++channel)
        {
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            if (output != nullptr)
                std::memset (output, 0, static_cast<std::size_t> (data.numSamples) * sizeof (Vst::Sample32));
        }
        data.outputs[bus].silenceFlags = ((uint64)1 << data.outputs[bus].numChannels) - 1;
    }
}

} // namespace CV
