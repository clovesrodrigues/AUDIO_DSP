#include "../../CV_DSP/Guitar/Pedals/WahWahDSP.hpp"

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

    cvdsp::guitar::pedals::WahWahDSP<float> wah;
    wah.prepare(48000.0f);
    wah.setMinFrequencyHz(250.0f);
    wah.setMaxFrequencyHz(2200.0f);
    wah.setMinQ(2.0f);
    wah.setMaxQ(5.5f);
    wah.setBandPassGain(2.0f);
    wah.setDryGain(0.2f);
    wah.setDrive(0.12f);
    wah.setLevelDb(-3.0f);
    wah.setMix(1.0f);

    float channel[kNumSamples] {};
    for (std::size_t i = 0; i < kNumSamples; ++i)
    {
        const float n = static_cast<float>(i);
        channel[i] = std::sin(n * 0.061f) * 0.2f + std::sin(n * 0.017f) * 0.08f;
    }

    for (std::size_t block = 0; block < 4; ++block)
    {
        wah.setExpression(static_cast<float>(block) / 3.0f);
        float* channels[] { channel + block * 64 };
        cvdsp::AudioBufferView<float> buffer(channels, 1, 64);
        wah.processBlock(buffer);
    }

    if (!isFiniteBuffer(channel, kNumSamples))
        return 1;

    float silence[64] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 64);
    wah.reset();
    wah.processBlock(silentBuffer);

    if (!isFiniteBuffer(silence, 64))
        return 2;

    const auto& descriptors = cvdsp::guitar::pedals::WahWahDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 3;
    }

    return descriptors.size() == 13 ? 0 : 4;
}
