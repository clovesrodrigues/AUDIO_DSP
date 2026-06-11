#include "../../CV_DSP/Effects/Phaser.hpp"
#include "../../CV_DSP/Guitar/Pedals/PhaserDSP.hpp"

#include <cmath>
#include <cstddef>

namespace
{
bool isFiniteBuffer(const float* buffer, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(buffer[i]))
            return false;
    }
    return true;
}
} // namespace

int main()
{
    constexpr std::size_t kNumSamples = 256;

    cvdsp::Phaser<float> generic;
    generic.prepare(48000.0f);
    generic.setRate(0.45f);
    generic.setDepth(0.9f);
    generic.setFeedback(0.35f);
    generic.setFrequencyRange(180.0f, 1800.0f);
    generic.setStages(4);
    generic.setMix(1.0f);

    float genericBuffer[kNumSamples] {};
    for (std::size_t i = 0; i < kNumSamples; ++i)
    {
        const float n = static_cast<float>(i);
        genericBuffer[i] = std::sin(n * 0.047f) * 0.25f + std::sin(n * 0.011f) * 0.08f;
    }

    for (float& sample : genericBuffer)
        sample = generic.process(sample);

    if (!isFiniteBuffer(genericBuffer, kNumSamples))
        return 1;

    cvdsp::guitar::pedals::PhaserDSP<float> pedal;
    pedal.prepare(48000.0f);
    pedal.setRateHz(0.35f);
    pedal.setDepth(0.85f);
    pedal.setFeedback(0.25f);
    pedal.setFrequencyRangeHz(250.0f, 1600.0f);
    pedal.setStages(4);
    pedal.setLevelDb(-3.0f);
    pedal.setMix(1.0f);

    float pedalBuffer[kNumSamples] {};
    for (std::size_t i = 0; i < kNumSamples; ++i)
    {
        const float n = static_cast<float>(i);
        pedalBuffer[i] = std::sin(n * 0.061f) * 0.2f + std::sin(n * 0.017f) * 0.08f;
    }

    float* channels[] { pedalBuffer };
    cvdsp::AudioBufferView<float> buffer(channels, 1, kNumSamples);
    pedal.processBlock(buffer);

    if (!isFiniteBuffer(pedalBuffer, kNumSamples))
        return 2;

    float silence[64] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 64);
    pedal.reset();
    pedal.processBlock(silentBuffer);

    if (!isFiniteBuffer(silence, 64))
        return 3;

    const auto& descriptors = cvdsp::guitar::pedals::PhaserDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 4;
    }

    return descriptors.size() == 11 ? 0 : 5;
}
