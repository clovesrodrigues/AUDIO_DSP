#include "SustainerVST2.hpp"

#include <algorithm>
#include <cstring>

namespace CV {
namespace {
using cvdsp::adapters::vst2::NormalizedValue;
using namespace cvdsp::guitar::pedals;
}

SustainerVST2::SustainerVST2(audioMasterCallback audioMaster)
    : VST2EffectBase(
        audioMaster,
        kNumPrograms,
        static_cast<VstInt32>(descriptors().size()),
        kUniqueID,
        "CV Sustainer",
        "Cloves Plugins",
        "CV Sustainer VST2",
        kVendorVersion)
{
    (void)initializeParametersFromDescriptors(descriptors());
}

void SustainerVST2::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    if (sampleFrames <= 0 || outputs == nullptr)
        return;

    for (std::size_t channel = 0; channel < processors_.size(); ++channel)
    {
        const float* input = inputs != nullptr ? inputs[channel] : nullptr;
        float* output = outputs[channel];
        if (output == nullptr)
            continue;

        if (input == nullptr)
        {
            copyOrClear(output, nullptr, sampleFrames);
            continue;
        }

        auto& processor = processors_[channel];
        for (VstInt32 sample = 0; sample < sampleFrames; ++sample)
            output[sample] = processor.processSample(input[sample]);
    }
}

const SustainerVST2::DescriptorArray& SustainerVST2::descriptors() noexcept
{
    return DSP::getParameterDescriptors();
}

float SustainerVST2::normalizedToReal(
    cvdsp::manager::ParameterID id,
    NormalizedValue normalizedValue) noexcept
{
    const float normalized = std::clamp(normalizedValue, 0.0f, 1.0f);
    for (const auto& descriptor : descriptors())
    {
        if (descriptor.getID() == id)
            return descriptor.normalizedToReal(normalized);
    }
    return normalized;
}

int SustainerVST2::normalizedToIndex(
    cvdsp::manager::ParameterID id,
    NormalizedValue normalizedValue,
    int maxIndex) noexcept
{
    const float realValue = normalizedToReal(id, normalizedValue);
    return std::clamp(static_cast<int>(realValue + 0.5f), 0, maxIndex);
}

void SustainerVST2::applyParameterToDSP(
    cvdsp::manager::ParameterID id,
    NormalizedValue normalized) noexcept
{
    const float realValue = normalizedToReal(id, normalized);

    for (auto& processor : processors_)
    {
        switch (id)
        {
            case PedalParameterIDs::Bypass:
                processor.setBypassed(realValue >= 0.5f);
                break;
            case PedalParameterIDs::InputGain:
                processor.setInputGainDb(realValue);
                break;
            case PedalParameterIDs::Sustain:
                processor.setSustain(normalized);
                break;
            case PedalParameterIDs::Attack:
                processor.setAttackMs(realValue);
                break;
            case PedalParameterIDs::Release:
                processor.setReleaseMs(realValue);
                break;
            case PedalParameterIDs::CompressionRatio:
                processor.setRatio(realValue);
                break;
            case PedalParameterIDs::MakeupGain:
                processor.setMakeupGainDb(realValue);
                break;
            case PedalParameterIDs::OutputLevel:
                processor.setLevelDb(realValue);
                break;
            case PedalParameterIDs::DryWetMix:
                processor.setMix(normalized);
                break;
            case PedalParameterIDs::GateEnable:
                processor.setGateEnabled(realValue >= 0.5f);
                break;
            case PedalParameterIDs::GateThreshold:
                processor.setGateThresholdDb(realValue);
                break;
            case PedalParameterIDs::GateRelease:
                processor.setGateReleaseMs(realValue);
                break;
            case PedalParameterIDs::SidechainHighPassFrequency:
                processor.setSidechainHighPassHz(realValue);
                break;
            case PedalParameterIDs::DetectorMode:
                processor.setDetectionMode(normalizedToIndex(id, normalized, 1) == 0
                    ? cvdsp::dynamics::EnvelopeMode::Peak
                    : cvdsp::dynamics::EnvelopeMode::RMS);
                break;
            case PedalParameterIDs::MaxBoost:
                processor.setMaxBoostDb(realValue);
                break;
            default:
                break;
        }
    }
}

void SustainerVST2::prepareDSP(float sampleRate, VstInt32 maxBlockSize) noexcept
{
    sampleRate_ = sampleRate > 0.0f ? sampleRate : 44100.0f;
    maxBlockSize_ = maxBlockSize > 0 ? maxBlockSize : 1024;
    (void)maxBlockSize_;

    for (auto& processor : processors_)
        processor.prepare(sampleRate_);

    applyAllParametersToDSP();
}

void SustainerVST2::resetDSP() noexcept
{
    for (auto& processor : processors_)
        processor.reset();
}

void SustainerVST2::copyOrClear(float* output, const float* input, VstInt32 sampleFrames) noexcept
{
    if (output == nullptr || sampleFrames <= 0)
        return;

    if (input == nullptr)
    {
        std::memset(output, 0, static_cast<std::size_t>(sampleFrames) * sizeof(float));
        return;
    }

    if (output != input)
        std::memcpy(output, input, static_cast<std::size_t>(sampleFrames) * sizeof(float));
}

} // namespace CV

