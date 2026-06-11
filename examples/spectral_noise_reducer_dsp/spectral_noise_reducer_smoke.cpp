#include "../../CV_DSP/Spectral/SpectralNoiseReducer.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSampleRate = 48000.0f;

float syntheticNoise(std::size_t sample) noexcept
{
    const float n = static_cast<float>(sample);
    const float hum60 = 0.040f * std::sin(2.0f * kPi * 60.0f * n / kSampleRate);
    const float hum180 = 0.018f * std::sin(2.0f * kPi * 180.0f * n / kSampleRate);
    const float hiss = 0.010f * std::sin(2.0f * kPi * 3100.0f * n / kSampleRate);
    return hum60 + hum180 + hiss;
}

bool isFinite(float value) noexcept
{
    return std::isfinite(value);
}

template<std::size_t Size>
bool isFiniteBuffer(const std::array<float, Size>& buffer) noexcept
{
    for (const float sample : buffer)
    {
        if (!isFinite(sample))
            return false;
    }
    return true;
}

template<std::size_t Size>
double rmsAfterWarmup(const std::array<float, Size>& buffer, std::size_t warmup) noexcept
{
    double energy = 0.0;
    std::size_t count = 0;
    for (std::size_t i = warmup; i < Size; ++i)
    {
        energy += static_cast<double>(buffer[i]) * static_cast<double>(buffer[i]);
        ++count;
    }

    return count > 0 ? std::sqrt(energy / static_cast<double>(count)) : 0.0;
}
} // namespace

int main()
{
    using Reducer = cvdsp::spectral::SpectralNoiseReducer<float, 512>;

    Reducer reducer;
    if (!reducer.prepare(kSampleRate) || !reducer.isPrepared())
    {
        std::cerr << "SpectralNoiseReducer prepare failed\n";
        return 1;
    }

    reducer.setMinimumLearnFrames(4);
    reducer.setReductionAmount(0.95f);
    reducer.setSpectralFloorDb(-72.0f);
    reducer.setMaxReductionDb(48.0f);
    reducer.setPresenceProtect(0.35f);
    reducer.setOutputGainDb(0.0f);
    reducer.setFrequencySmoothingBins(3);
    reducer.setTransientProtection(0.4f);
    reducer.setMix(1.0f);

    if (reducer.getFrequencySmoothingBins() != 3
        || std::fabs(reducer.getTransientProtection() - 0.4f) > 1.0e-6f)
    {
        std::cerr << "Advanced smoothing parameters were not applied\n";
        return 13;
    }

    // Perceber Ruido: enabled. Subtrair Ruidos: disabled while capturing a clean noise-only profile.
    reducer.setLearnNoiseEnabled(true);
    reducer.setSubtractNoiseEnabled(false);

    constexpr std::size_t kLearnSamples = 8192;
    for (std::size_t i = 0; i < kLearnSamples; ++i)
    {
        const float learned = reducer.processSample(syntheticNoise(i));
        if (!isFinite(learned))
        {
            std::cerr << "Non-finite learn sample\n";
            return 2;
        }
    }

    if (!reducer.isProfileReady() || reducer.getLearnProgress() < 1.0f)
    {
        std::cerr << "Noise profile was not learned\n";
        return 3;
    }

    // Recording workflow: Perceber Ruido off, Subtrair Ruidos on.
    reducer.setLearnNoiseEnabled(false);
    reducer.setSubtractNoiseEnabled(true);

    constexpr std::size_t kProcessSamples = 16384;
    constexpr std::size_t kWarmupSamples = Reducer::kFFTSize * 3;
    std::array<float, kProcessSamples> input {};
    std::array<float, kProcessSamples> output {};

    for (std::size_t i = 0; i < kProcessSamples; ++i)
    {
        input[i] = syntheticNoise(i);
        output[i] = reducer.processSample(input[i]);
    }

    if (!isFiniteBuffer(output))
    {
        std::cerr << "Non-finite subtract output\n";
        return 4;
    }

    const double inputRms = rmsAfterWarmup(input, kWarmupSamples);
    const double outputRms = rmsAfterWarmup(output, kWarmupSamples);
    if (!(outputRms < inputRms * 0.75))
    {
        std::cerr << "Insufficient reduction: inputRms=" << inputRms
                  << " outputRms=" << outputRms << '\n';
        return 5;
    }

    const auto& inputSpectrum = reducer.getInputSpectrum();
    const auto& noiseProfile = reducer.getNoiseProfile();
    const auto& outputSpectrum = reducer.getOutputSpectrum();
    if (inputSpectrum[1] <= 0.0f || noiseProfile[1] <= 0.0f || outputSpectrum[1] < 0.0f)
    {
        std::cerr << "Invalid spectral snapshots\n";
        return 6;
    }

    Reducer::GuiSnapshot guiSnapshot {};
    reducer.fillGuiSnapshot(guiSnapshot);
    if (!guiSnapshot.profileReady || !guiSnapshot.subtracting || guiSnapshot.learnProgress < 1.0f)
    {
        std::cerr << "Invalid GUI state snapshot\n";
        return 7;
    }

    if (!isFinite(guiSnapshot.inputDb[1]) || !isFinite(guiSnapshot.noiseProfileDb[1])
        || !isFinite(guiSnapshot.outputDb[1]) || !isFinite(guiSnapshot.reductionDb[1]))
    {
        std::cerr << "Invalid GUI dB snapshot\n";
        return 8;
    }

    if (guiSnapshot.inputNormalized[1] < 0.0f || guiSnapshot.inputNormalized[1] > 1.0f
        || guiSnapshot.outputNormalized[1] < 0.0f || guiSnapshot.outputNormalized[1] > 1.0f)
    {
        std::cerr << "Invalid GUI normalized snapshot\n";
        return 9;
    }

    if (reducer.getBinFrequencyHz(1) <= 0.0f)
    {
        std::cerr << "Invalid GUI bin frequency\n";
        return 10;
    }

    // Limpar Perfil: clear learned profile and verify subtract cannot remain active with stale data.
    reducer.triggerClearProfile();
    if (reducer.isProfileReady() || reducer.getLearnedFrameCount() != 0)
    {
        std::cerr << "Profile clear failed\n";
        return 11;
    }

    // Both switches off: hard bypass for safe insertion at the beginning of a recording chain.
    reducer.setLearnNoiseEnabled(false);
    reducer.setSubtractNoiseEnabled(false);
    const float bypassed = reducer.processSample(0.125f);
    if (std::fabs(bypassed - 0.125f) > 1.0e-7f)
    {
        std::cerr << "Bypass changed the sample\n";
        return 12;
    }

    std::cout << "SpectralNoiseReducer smoke passed. inputRms=" << inputRms
              << " outputRms=" << outputRms << '\n';
    return 0;
}
