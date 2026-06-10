#include "../../CV_DSP/Guitar/Pedals.hpp"

#include <cmath>
#include <cstddef>

namespace
{
template<typename Pedal>
bool processPedal(Pedal& pedal, float* channel, std::size_t count)
{
    float* channels[] { channel };
    cvdsp::AudioBufferView<float> buffer(channels, 1, count);
    pedal.processBlock(buffer);

    for (std::size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(channel[i]))
            return false;
    }

    return true;
}

bool descriptorsAreValid()
{
    for (const auto& descriptor : cvdsp::guitar::pedals::SustainerDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }
    for (const auto& descriptor : cvdsp::guitar::pedals::WahWahDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }
    for (const auto& descriptor : cvdsp::guitar::pedals::PhaserDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }
    for (const auto& descriptor : cvdsp::guitar::pedals::ClassicOverdriveDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }
    for (const auto& descriptor : cvdsp::guitar::pedals::VintageHardDistortionDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }
    for (const auto& descriptor : cvdsp::guitar::pedals::VintageFuzzDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }
    for (const auto& descriptor : cvdsp::guitar::pedals::ChainsawMetalDSP<float>::getParameterDescriptors())
    {
        if (!descriptor.isValid())
            return false;
    }

    return true;
}
} // namespace

int main()
{
    constexpr std::size_t kNumSamples = 128;
    float source[kNumSamples] {};

    for (std::size_t i = 0; i < kNumSamples; ++i)
    {
        const float n = static_cast<float>(i);
        source[i] = std::sin(n * 0.071f) * 0.25f + std::sin(n * 0.013f) * 0.1f;
    }

    cvdsp::guitar::pedals::SustainerDSP<float> sustainer;
    cvdsp::guitar::pedals::WahWahDSP<float> wah;
    cvdsp::guitar::pedals::PhaserDSP<float> phaser;
    cvdsp::guitar::pedals::ClassicOverdriveDSP<float> overdrive;
    cvdsp::guitar::pedals::VintageHardDistortionDSP<float> distortion;
    cvdsp::guitar::pedals::VintageFuzzDSP<float> fuzz;
    cvdsp::guitar::pedals::ChainsawMetalDSP<float> chainsaw;

    sustainer.prepare(48000.0f);
    wah.prepare(48000.0f);
    phaser.prepare(48000.0f);
    overdrive.prepare(48000.0f);
    distortion.prepare(48000.0f);
    fuzz.prepare(48000.0f);
    chainsaw.prepare(48000.0f);

    sustainer.setSustain(0.65f);
    sustainer.setLevelDb(-6.0f);

    wah.setExpression(0.55f);
    wah.setLevelDb(-3.0f);

    phaser.setRateHz(0.35f);
    phaser.setDepth(0.85f);
    phaser.setFeedback(0.25f);
    phaser.setLevelDb(-3.0f);

    overdrive.setDrive(0.45f);
    overdrive.setTone(0.55f);
    overdrive.setLevelDb(-3.0f);

    distortion.setDistortion(0.7f);
    distortion.setScoopAmount(0.55f);
    distortion.setLevelDb(-6.0f);

    fuzz.setFuzz(0.8f);
    fuzz.setBias(0.15f);
    fuzz.setStarve(0.25f);
    fuzz.setLevelDb(-8.0f);

    chainsaw.setVoiceMode(cvdsp::guitar::pedals::ChainsawVoiceMode::ClassicSwedish);
    chainsaw.setGain(0.85f);
    chainsaw.setLevelDb(-14.0f);

    float buffer[kNumSamples] {};

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(sustainer, buffer, kNumSamples))
        return 1;

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(wah, buffer, kNumSamples))
        return 2;

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(phaser, buffer, kNumSamples))
        return 3;

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(overdrive, buffer, kNumSamples))
        return 4;

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(distortion, buffer, kNumSamples))
        return 5;

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(fuzz, buffer, kNumSamples))
        return 6;

    for (std::size_t i = 0; i < kNumSamples; ++i)
        buffer[i] = source[i];
    if (!processPedal(chainsaw, buffer, kNumSamples))
        return 7;

    if (!descriptorsAreValid())
        return 8;

    return 0;
}
