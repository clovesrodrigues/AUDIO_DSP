//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "CV_DSP/Guitar/Pedals/PedalParameterIDs.hpp"
#include "base/source/fstreamer.h"

#include <algorithm>
#include <cstring>
#include <tuple>

using namespace Steinberg;

namespace CV {
namespace {
using namespace cvdsp::guitar::pedals;
}

VintageFuzzVST3Processor::VintageFuzzVST3Processor ()
{
    setControllerClass (kVintageFuzzVST3ControllerUID);
    (void)parameterState_.initializeFromDescriptors (descriptors ());
    applyAllParametersToDSP ();
}

VintageFuzzVST3Processor::~VintageFuzzVST3Processor ()
{}

tresult PLUGIN_API VintageFuzzVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API VintageFuzzVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API VintageFuzzVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& processor : processors_)
            processor.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API VintageFuzzVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& processor : processors_)
        processor.prepare (static_cast<float> (sampleRate_));
    applyAllParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API VintageFuzzVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API VintageFuzzVST3Processor::process (Vst::ProcessData& data)
{
    if (data.symbolicSampleSize != Vst::kSample32)
        return kResultFalse;

    parameterState_.applyParameterChanges (data.inputParameterChanges,
        [this](Vst::ParamID id, Vst::ParamValue normalizedValue) noexcept {
            applyParameterToDSP (id, normalizedValue);
        });

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
            auto& processor = processors_[static_cast<std::size_t> (channel)];

            if (input == nullptr || output == nullptr)
                continue;

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

tresult PLUGIN_API VintageFuzzVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (const auto& descriptor : descriptors ())
    {
        float normalized = static_cast<float> (CV::Pedais::defaultNormalizedForDescriptor (descriptor));
        if (!streamer.readFloat (normalized))
            return kResultFalse;
        (void)parameterState_.setNormalized (descriptor.getID (), normalized);
    }

    applyAllParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API VintageFuzzVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (const auto& descriptor : descriptors ())
    {
        const auto normalized = static_cast<float> (parameterState_.getNormalized (
            descriptor.getID (),
            CV::Pedais::defaultNormalizedForDescriptor (descriptor)));
        if (!streamer.writeFloat (normalized))
            return kResultFalse;
    }
    return kResultOk;
}

const VintageFuzzVST3Processor::DescriptorArray& VintageFuzzVST3Processor::descriptors () noexcept
{
    return DSP::getParameterDescriptors ();
}

void VintageFuzzVST3Processor::applyAllParametersToDSP () noexcept
{
    for (const auto& descriptor : descriptors ())
    {
        applyParameterToDSP (
            descriptor.getID (),
            parameterState_.getNormalized (descriptor.getID (), CV::Pedais::defaultNormalizedForDescriptor (descriptor)));
    }
}

void VintageFuzzVST3Processor::applyParameterToDSP (
    Vst::ParamID id,
    Vst::ParamValue normalizedValue) noexcept
{
    const float realValue = normalizedToReal (id, normalizedValue);

    for (auto& processor : processors_)
    {
        switch (id)
        {
            case PedalParameterIDs::Bypass:
                processor.setBypassed (realValue >= 0.5f);
                break;
            case PedalParameterIDs::InputGain:
                processor.setInputGainDb (realValue);
                break;
            case PedalParameterIDs::Drive:
                processor.setFuzz (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::OutputLevel:
                processor.setLevelDb (realValue);
                break;
            case PedalParameterIDs::Tone:
                processor.setTone (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::DryWetMix:
                processor.setMix (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::Oversampling:
                processor.setOversamplingMode (static_cast<PedalOversamplingMode> (normalizedToIndex (id, normalizedValue, 3)));
                break;
            case PedalParameterIDs::QualityMode:
                processor.setQualityMode (static_cast<PedalQualityMode> (normalizedToIndex (id, normalizedValue, 2)));
                break;
            case PedalParameterIDs::Bias:
                processor.setBias (realValue);
                break;
            case PedalParameterIDs::InterstageGain:
                processor.setStarve (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::ClipBlend:
                processor.setCleanup (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::PreLowPassFrequency:
                processor.setInputLoad (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::Asymmetry:
                processor.setAsymmetry (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::PositiveThreshold:
                processor.setPositiveGain (realValue);
                break;
            case PedalParameterIDs::NegativeThreshold:
                processor.setNegativeGain (realValue);
                break;
            case PedalParameterIDs::FoldbackAmount:
                processor.setFoldbackAmount (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::RectifyMode:
                processor.setRectifyMode (static_cast<PedalRectifyMode> (normalizedToIndex (id, normalizedValue, 2)));
                break;
            case PedalParameterIDs::PreHighPassFrequency:
                processor.core ().preFilter ().setHighPassFrequency (realValue);
                break;
            case PedalParameterIDs::PostLowPassFrequency:
                processor.setPostLowPassHz (realValue);
                break;
            case PedalParameterIDs::Bass:
                processor.setLowBloom (static_cast<float> (normalizedValue));
                break;
            case PedalParameterIDs::GateEnable:
                processor.setGateEnabled (realValue >= 0.5f);
                break;
            case PedalParameterIDs::GateThreshold:
                processor.setGateThresholdDb (realValue);
                break;
            default:
                break;
        }
    }
}

float VintageFuzzVST3Processor::normalizedToReal (
    Vst::ParamID id,
    Vst::ParamValue normalizedValue) noexcept
{
    const float normalized = std::clamp (static_cast<float> (normalizedValue), 0.0f, 1.0f);
    for (const auto& descriptor : descriptors ())
    {
        if (descriptor.getID () == id)
            return descriptor.normalizedToReal (normalized);
    }
    return normalized;
}

int VintageFuzzVST3Processor::normalizedToIndex (
    Vst::ParamID id,
    Vst::ParamValue normalizedValue,
    int maxIndex) noexcept
{
    const float realValue = normalizedToReal (id, normalizedValue);
    return std::clamp (static_cast<int> (realValue + 0.5f), 0, maxIndex);
}

void VintageFuzzVST3Processor::clearRemainingOutputs (Vst::ProcessData& data, int32 firstBus) noexcept
{
    for (int32 bus = firstBus; bus < data.numOutputs; ++bus)
    {
        for (int32 channel = 0; channel < data.outputs[bus].numChannels; ++channel)
        {
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            if (output != nullptr)
            {
                std::memset (
                    output,
                    0,
                    static_cast<std::size_t> (data.numSamples) * sizeof (Vst::Sample32));
            }
        }
        data.outputs[bus].silenceFlags = ((uint64)1 << data.outputs[bus].numChannels) - 1;
    }
}

} // namespace CV
