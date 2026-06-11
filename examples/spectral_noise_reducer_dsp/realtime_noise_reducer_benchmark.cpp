#include "../../CV_DSP/Spectral/RealtimeNoiseReducer.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSampleRate = 48000.0f;
constexpr std::size_t kSeconds = 3;
constexpr std::size_t kSamples = static_cast<std::size_t>(kSampleRate) * kSeconds;

float noiseForTrack(std::size_t track, std::size_t sample) noexcept
{
    const float n = static_cast<float>(sample);
    const float base = 60.0f + static_cast<float>(track * 37);
    return 0.028f * std::sin(2.0f * kPi * base * n / kSampleRate)
        + 0.012f * std::sin(2.0f * kPi * (base * 3.0f) * n / kSampleRate)
        + 0.006f * std::sin(2.0f * kPi * (2500.0f + static_cast<float>(track * 113)) * n / kSampleRate);
}

template<std::size_t InstanceCount>
bool runBenchmarkGroup()
{
    using Reducer = cvdsp::spectral::RealtimeNoiseReducer<float, 1024>;
    std::array<Reducer, InstanceCount> reducers {};

    for (auto& reducer : reducers)
    {
        if (!reducer.prepare(kSampleRate))
            return false;
        reducer.setMinimumLearnFrames(4);
        reducer.setPresenceProtect(0.5f);
        reducer.setSmoothing(0.65f);
        reducer.setOutputGainDb(0.0f);
        reducer.setLearnNoiseEnabled(true);
    }

    for (std::size_t sample = 0; sample < Reducer::kFFTSize * 8; ++sample)
    {
        for (std::size_t track = 0; track < InstanceCount; ++track)
            (void)reducers[track].processSample(noiseForTrack(track, sample));
    }

    for (auto& reducer : reducers)
    {
        if (!reducer.isProfileReady())
            return false;
        reducer.setLearnNoiseEnabled(false);
        reducer.setSubtractNoiseEnabled(true);
        reducer.resetLatencyState();
    }

    float checksum = 0.0f;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t sample = 0; sample < kSamples; ++sample)
    {
        for (std::size_t track = 0; track < InstanceCount; ++track)
        {
            const float musicalSignal = 0.045f * std::sin(2.0f * kPi * (180.0f + static_cast<float>(track * 11))
                                                         * static_cast<float>(sample) / kSampleRate);
            const float output = reducers[track].processSample(musicalSignal + noiseForTrack(track, sample));
            if (!std::isfinite(output))
                return false;
            checksum += output * 0.000001f;
        }
    }
    const auto stop = std::chrono::steady_clock::now();

    const double elapsedSeconds = std::chrono::duration<double>(stop - start).count();
    const double audioSeconds = static_cast<double>(kSeconds) * static_cast<double>(InstanceCount);
    const double realtimeFactor = audioSeconds / elapsedSeconds;

    std::cout << InstanceCount << " instance(s): " << elapsedSeconds << " s CPU for "
              << audioSeconds << " s audio, realtime factor " << realtimeFactor
              << ", checksum " << checksum << '\n';
    return true;
}
} // namespace

int main()
{
    if (!runBenchmarkGroup<1>())
        return 1;
    if (!runBenchmarkGroup<4>())
        return 2;
    if (!runBenchmarkGroup<10>())
        return 3;
    return 0;
}
