#include "../../CV_DSP/Guitar/Pedals/ClassicOverdriveDSP.hpp"

#include <cmath>
#include <cstddef>

int main()
{
    cvdsp::guitar::pedals::ClassicOverdriveDSP<float> overdrive;
    overdrive.prepare(48000.0f);
    overdrive.setDrive(0.65f);
    overdrive.setTone(0.55f);
    overdrive.setLevelDb(-3.0f);
    overdrive.setMix(1.0f);

    float channel[64] {};
    for (std::size_t i = 0; i < 64; ++i)
        channel[i] = std::sin(static_cast<float>(i) * 0.1f) * 0.25f;

    float* channels[] { channel };
    cvdsp::AudioBufferView<float> buffer(channels, 1, 64);
    overdrive.processBlock(buffer);

    for (const float sample : channel)
    {
        if (!std::isfinite(sample))
            return 1;
    }

    float silence[16] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 16);
    overdrive.reset();
    overdrive.processBlock(silentBuffer);

    for (const float sample : silence)
    {
        if (!std::isfinite(sample))
            return 2;
    }

    const auto& descriptors = cvdsp::guitar::pedals::ClassicOverdriveDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 3;
    }

    return descriptors.size() == 14 ? 0 : 4;
}
