#include "../../CV_DSP/Guitar/Pedals/ChainsawMetalDSP.hpp"

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

bool processCase(
    cvdsp::guitar::pedals::ChainsawVoiceMode voice,
    cvdsp::guitar::pedals::PedalOversamplingMode oversampling,
    float gain,
    float threshold,
    float lowMidGain,
    float highMidGain)
{
    cvdsp::guitar::pedals::ChainsawMetalDSP<float> pedal;
    pedal.prepare(48000.0f);
    pedal.setVoiceMode(voice);
    pedal.setOversamplingMode(oversampling);
    pedal.setGain(gain);
    pedal.setHardThreshold(threshold);
    pedal.setLowMidGainDb(lowMidGain);
    pedal.setHighMidGainDb(highMidGain);
    pedal.setGateEnabled(true);
    pedal.setGateThresholdDb(-85.0f);
    pedal.setLevelDb(-12.0f);

    float channel[192] {};
    for (std::size_t i = 0; i < 192; ++i)
    {
        const float n = static_cast<float>(i);
        channel[i] = std::sin(n * 0.11f) * 0.35f + std::sin(n * 0.037f) * 0.2f;
    }

    float* channels[] { channel };
    cvdsp::AudioBufferView<float> buffer(channels, 1, 192);
    pedal.processBlock(buffer);

    if (!isFiniteBuffer(channel, 192))
        return false;

    float peak = 0.0f;
    for (const float sample : channel)
        peak = std::max(peak, std::abs(sample));
    if (!std::isfinite(peak) || peak > 50.0f)
        return false;

    float silence[128] {};
    float* silentChannels[] { silence };
    cvdsp::AudioBufferView<float> silentBuffer(silentChannels, 1, 128);
    pedal.reset();
    pedal.processBlock(silentBuffer);

    return isFiniteBuffer(silence, 128);
}
} // namespace

int main()
{
    using cvdsp::guitar::pedals::ChainsawVoiceMode;
    using cvdsp::guitar::pedals::PedalOversamplingMode;

    if (!processCase(ChainsawVoiceMode::ClassicSwedish, PedalOversamplingMode::Off, 0.9f, 0.3f, 12.0f, 15.0f))
        return 1;
    if (!processCase(ChainsawVoiceMode::ModernTight, PedalOversamplingMode::x2, 1.0f, 0.22f, 9.0f, 12.0f))
        return 2;
    if (!processCase(ChainsawVoiceMode::DoomLoose, PedalOversamplingMode::x4, 0.85f, 0.4f, 18.0f, 10.0f))
        return 3;
    if (!processCase(ChainsawVoiceMode::DeathMetalScoop, PedalOversamplingMode::x8, 1.0f, 0.18f, 8.0f, 24.0f))
        return 4;

    const auto& descriptors = cvdsp::guitar::pedals::ChainsawMetalDSP<float>::getParameterDescriptors();
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid())
            return 5;
    }

    return descriptors.size() == 27 ? 0 : 6;
}
