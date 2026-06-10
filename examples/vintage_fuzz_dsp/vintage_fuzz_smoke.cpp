#include "../../CV_DSP/Guitar/Pedals/VintageFuzzDSP.hpp"

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

float meanValue(const float* buffer, std::size_t count)
{
    float sum = 0.0f;
    for (std::size_t i = 0; i < count; ++i)
        sum += buffer[i];
    return count > 0 ? sum / static_cast<float>(count) : 0.0f;
}

bool processExtremeCase(float fuzz, float bias, float starve, float asymmetry,
    cvdsp::guitar::pedals::PedalRectifyMode rectifyMode)
{
    cvdsp::guitar::pedals::VintageFuzzDSP<float> pedal;
    pedal.prepare(48000.0f);
    pedal.setFuzz(fuzz);
    pedal.setBias(bias);
    pedal.setStarve(starve);
    pedal.setAsymmetry(asymmetry);
    pedal.setFoldbackAmount(starve);
    pedal.setRectifyMode(rectifyMode);
    pedal.setOversamplingMode(cvdsp::guitar::pedals::PedalOversamplingMode::x8);
    pedal.setGateEnabled(true);
    pedal.setGateThresholdDb(-80.0f);

    float channel[256] {};
    for (std::size_t i = 0; i < 256; ++i)
    {
        const float n = static_cast<float>(i);
        channel[i] = std::sin(n * 0.071f) * 0.45f + std::sin(n * 0.019f) * 0.2f;
    }

    float* channels[] { channel };
    cvdsp::AudioBufferView<float> buffer(channels, 1, 256);
    pedal.processBlock(buffer);

    if (!isFiniteBuffer(channel, 256))
        return false;

    const float dc = meanValue(channel, 256);
    if (!std::isfinite(dc) || std::abs(dc) > 0.6f)
        return false;

    float silence[4096] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 4096);
    pedal.reset();
    pedal.processBlock(silentBuffer);

    return isFiniteBuffer(silence, 4096) && std::abs(meanValue(silence + 3072, 1024)) < 0.01f;
}
} // namespace

int main()
{
    using cvdsp::guitar::pedals::PedalRectifyMode;

    if (!processExtremeCase(0.2f, -0.25f, 0.0f, 0.2f, PedalRectifyMode::Off))
        return 1;
    if (!processExtremeCase(0.85f, 0.35f, 0.5f, 0.8f, PedalRectifyMode::Half))
        return 2;
    if (!processExtremeCase(1.0f, 1.0f, 1.0f, 1.0f, PedalRectifyMode::Full))
        return 3;

    const auto& descriptors = cvdsp::guitar::pedals::VintageFuzzDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 4;
    }

    return descriptors.size() == 22 ? 0 : 5;
}
