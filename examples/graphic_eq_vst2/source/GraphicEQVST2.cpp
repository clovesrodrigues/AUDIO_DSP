#include "GraphicEQVST2.hpp"

#include <algorithm>
#include <cstring>

namespace CV {
namespace {

constexpr cvdsp::manager::ParameterID kGraphicEQBaseParamID = 2000;
constexpr cvdsp::manager::ParameterFlags kGainParameterFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;

constexpr std::array<const char*, 10> kGainNames {
    "31 Hz", "63 Hz", "125 Hz", "250 Hz", "500 Hz",
    "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"
};

constexpr std::array<const char*, 10> kGainIDs {
    "graphic_eq.31_hz", "graphic_eq.63_hz", "graphic_eq.125_hz", "graphic_eq.250_hz", "graphic_eq.500_hz",
    "graphic_eq.1_khz", "graphic_eq.2_khz", "graphic_eq.4_khz", "graphic_eq.8_khz", "graphic_eq.16_khz"
};

constexpr cvdsp::manager::ParameterID gainParameterID(const std::size_t band) noexcept
{
    return kGraphicEQBaseParamID + static_cast<cvdsp::manager::ParameterID>(band);
}

constexpr cvdsp::manager::ParameterDescriptor<float> makeGainDescriptor(const std::size_t band) noexcept
{
    using namespace cvdsp::manager;
    return ParameterDescriptor<float>(
        gainParameterID(band),
        "Gain",
        kGainNames[band],
        ParameterUnit::Decibels,
        ParameterScale::Decibel,
        kGainParameterFlags,
        {-24.0f, 24.0f, 0.0f, 0.0f, 1.0f},
        nullptr,
        0,
        kGainIDs[band],
        "dB",
        "Graphic EQ",
        2);
}

} // namespace

GraphicEQVST2::GraphicEQVST2(audioMasterCallback audioMaster)
    : VST2EffectBase(
        audioMaster,
        kNumPrograms,
        static_cast<VstInt32>(descriptors().size()),
        kUniqueID,
        "CV Graphic EQ",
        "Cloves Plugins",
        "CV Graphic EQ VST2",
        kVendorVersion)
{
    (void)initializeParametersFromDescriptors(descriptors());
}

void GraphicEQVST2::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    if (sampleFrames <= 0 || outputs == nullptr)
        return;

    const auto byteCount = static_cast<std::size_t>(sampleFrames) * sizeof(float);
    for (std::size_t channel = 0; channel < eqs_.size(); ++channel)
    {
        const float* input = inputs != nullptr ? inputs[channel] : nullptr;
        float* output = outputs[channel];
        if (output == nullptr)
            continue;

        if (input == nullptr)
        {
            std::memset(output, 0, byteCount);
            continue;
        }

        if (output != input)
            std::memcpy(output, input, byteCount);
        eqs_[channel].processBlock(output, static_cast<std::size_t>(sampleFrames));
    }
}

const GraphicEQVST2::DescriptorArray& GraphicEQVST2::descriptors() noexcept
{
    static const DescriptorArray descriptors = [] {
        DescriptorArray values {};
        for (std::size_t band = 0; band < values.size(); ++band)
            values[band] = makeGainDescriptor(band);
        return values;
    }();
    return descriptors;
}

float GraphicEQVST2::normalizedToReal(
    cvdsp::manager::ParameterID id,
    cvdsp::adapters::vst2::NormalizedValue normalizedValue) noexcept
{
    const float normalized = std::clamp(normalizedValue, 0.0f, 1.0f);
    for (const auto& descriptor : descriptors())
    {
        if (descriptor.getID() == id)
            return descriptor.normalizedToReal(normalized);
    }
    return normalized;
}

void GraphicEQVST2::applyParameterToDSP(
    cvdsp::manager::ParameterID id,
    cvdsp::adapters::vst2::NormalizedValue normalized) noexcept
{
    if (id < kGraphicEQBaseParamID || id >= kGraphicEQBaseParamID + kBandCount)
        return;

    const auto band = static_cast<std::size_t>(id - kGraphicEQBaseParamID);
    gainsDB_[band] = normalizedToReal(id, normalized);
    for (auto& eq : eqs_)
        (void)eq.setBandGainDB(band, gainsDB_[band]);
}

void GraphicEQVST2::prepareDSP(float sampleRate, VstInt32 /*maxBlockSize*/) noexcept
{
    const float activeSampleRate = sampleRate > 0.0f ? sampleRate : 44100.0f;
    for (auto& eq : eqs_)
        eq.prepare(activeSampleRate);

    applyAllParametersToDSP();
}

void GraphicEQVST2::resetDSP() noexcept
{
    for (auto& eq : eqs_)
        eq.reset();
}

} // namespace CV
