#include "../../CV_DSP/Guitar/Pedals/SustainerDSP.hpp"

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

    cvdsp::guitar::pedals::SustainerDSP<float> sustainer;
    sustainer.prepare(48000.0f);
    sustainer.setSustain(0.7f);
    sustainer.setAttackMs(4.0f);
    sustainer.setReleaseMs(900.0f);
    sustainer.setGateThresholdDb(-64.0f);
    sustainer.setGateReleaseMs(90.0f);
    sustainer.setSidechainHighPassHz(80.0f);
    sustainer.setLevelDb(-6.0f);
    sustainer.setMix(1.0f);

    float note[kNumSamples] {};
    for (std::size_t i = 0; i < kNumSamples; ++i)
    {
        const float n = static_cast<float>(i);
        const float decay = std::exp(-n / 180.0f);
        note[i] = std::sin(n * 0.071f) * 0.22f * decay;
    }

    float* noteChannels[] { note };
    cvdsp::AudioBufferView<float> noteBuffer(noteChannels, 1, kNumSamples);
    sustainer.processBlock(noteBuffer);

    if (!isFiniteBuffer(note, kNumSamples))
        return 1;

    float silence[64] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 64);
    sustainer.reset();
    sustainer.processBlock(silentBuffer);

    if (!isFiniteBuffer(silence, 64))
        return 2;

    float noise[64] {};
    for (std::size_t i = 0; i < 64; ++i)
        noise[i] = (i % 2 == 0 ? 1.0f : -1.0f) * 0.00002f;

    float* noiseChannels[] { noise };
    cvdsp::AudioBufferView<float> noiseBuffer(noiseChannels, 1, 64);
    sustainer.reset();
    sustainer.processBlock(noiseBuffer);

    if (!isFiniteBuffer(noise, 64))
        return 3;

    const auto& descriptors = cvdsp::guitar::pedals::SustainerDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 4;
    }

    return descriptors.size() == 15 ? 0 : 5;
}
