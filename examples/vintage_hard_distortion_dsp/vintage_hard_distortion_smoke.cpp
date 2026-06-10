#include "../../CV_DSP/Guitar/Pedals/VintageHardDistortionDSP.hpp"

#include <cmath>
#include <cstddef>

namespace
{
bool processAndValidate(cvdsp::guitar::pedals::PedalOversamplingMode mode)
{
    cvdsp::guitar::pedals::VintageHardDistortionDSP<float> distortion;
    distortion.prepare(48000.0f);
    distortion.setOversamplingMode(mode);
    distortion.setDistortion(0.75f);
    distortion.setTone(0.6f);
    distortion.setLevelDb(-6.0f);
    distortion.setScoopAmount(0.65f);
    distortion.setScoopFrequencyHz(420.0f);
    distortion.setScoopQ(1.4f);
    distortion.setFizzCutHz(6500.0f);

    float channel[96] {};
    for (std::size_t i = 0; i < 96; ++i)
    {
        const float n = static_cast<float>(i);
        channel[i] = std::sin(n * 0.13f) * 0.35f + std::sin(n * 0.031f) * 0.15f;
    }

    float* channels[] { channel };
    cvdsp::AudioBufferView<float> buffer(channels, 1, 96);
    distortion.processBlock(buffer);

    for (const float sample : channel)
    {
        if (!std::isfinite(sample))
            return false;
    }

    float silence[24] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 24);
    distortion.reset();
    distortion.processBlock(silentBuffer);

    for (const float sample : silence)
    {
        if (!std::isfinite(sample))
            return false;
    }

    return true;
}
} // namespace

int main()
{
    using cvdsp::guitar::pedals::PedalOversamplingMode;

    if (!processAndValidate(PedalOversamplingMode::Off))
        return 1;
    if (!processAndValidate(PedalOversamplingMode::x2))
        return 2;
    if (!processAndValidate(PedalOversamplingMode::x4))
        return 3;
    if (!processAndValidate(PedalOversamplingMode::x8))
        return 4;

    const auto& descriptors = cvdsp::guitar::pedals::VintageHardDistortionDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 5;
    }

    return descriptors.size() == 19 ? 0 : 6;
}
