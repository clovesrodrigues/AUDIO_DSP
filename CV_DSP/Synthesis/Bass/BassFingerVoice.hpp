#ifndef CVDSP_SYNTHESIS_BASS_BASSFINGERVOICE_HPP
#define CVDSP_SYNTHESIS_BASS_BASSFINGERVOICE_HPP

/**
 * @file BassFingerVoice.hpp
 * @brief Lightweight monophonic electric bass finger voice foundation.
 *
 * Header-only
 * C++20
 * Real-time safe
 * No dynamic allocation
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

#include "../../Core/Types.hpp"
#include "../../Dynamics/Compressor.hpp"
#include "../../Filters/Biquad.hpp"
#include "../../Filters/StateVariableFilter.hpp"
#include "../../Modulation/ADSR.hpp"
#include "../../Modulation/Oscillator.hpp"
#include "../../Saturation/Waveshaper.hpp"

namespace cvdsp::synthesis::bass
{

/**
 * @brief First lightweight bass voice used by CV Bass Finger Lite.
 *
 * This class intentionally keeps the first playable stage small: one main
 * triangle oscillator, one sine sub oscillator, one amplitude ADSR, one
 * low-pass dynamic filter, light drive, compressor, 3-band EQ, subtle finger
 * noises, and lightweight deterministic humanization.
 */
template<typename T = f32>
class BassFingerVoice
{
    static_assert(std::is_floating_point_v<T>, "BassFingerVoice requires floating point type");

public:
    constexpr BassFingerVoice() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = std::max(sampleRate, static_cast<T>(1));

        mainOscillator_.prepare(
            sampleRate_,
            frequency_,
            cvdsp::modulation::OscillatorWaveform::Triangle);

        subOscillator_.prepare(
            sampleRate_,
            frequency_ * static_cast<T>(0.5),
            cvdsp::modulation::OscillatorWaveform::Sine);

        amplitudeEnvelope_.prepare(
            sampleRate_,
            attackMs_,
            static_cast<T>(85),
            static_cast<T>(0.72),
            static_cast<T>(120));

        toneFilter_.prepare(sampleRate_, cvdsp::filters::SVFMode::LowPass);
        toneFilter_.setResonance(static_cast<T>(0.72));

        compressor_.prepare(sampleRate_);
        compressor_.setAttackMs(static_cast<T>(8));
        compressor_.setReleaseMs(static_cast<T>(90));
        compressor_.setKneeDB(static_cast<T>(6));

        bassEq_.prepare(sampleRate_);
        midEq_.prepare(sampleRate_);
        trebleEq_.prepare(sampleRate_);
        configureEqTypes();
        updateNoiseDecays();
        updateToneFilter();
        updateCompressor();
        updateEq();

        reset();
    }

    void reset() noexcept
    {
        mainOscillator_.reset();
        subOscillator_.reset();
        amplitudeEnvelope_.reset();
        toneFilter_.reset();
        compressor_.reset();
        bassEq_.reset();
        midEq_.reset();
        trebleEq_.reset();
        active_ = false;
        currentNote_ = 0;
        velocityGain_ = static_cast<T>(0);
        normalizedVelocity_ = static_cast<T>(0);
        humanizedGain_ = static_cast<T>(1);
        humanizedCutoffOffset_ = static_cast<T>(0);
        attackNoiseEnvelope_ = static_cast<T>(0);
        releaseNoiseEnvelope_ = static_cast<T>(0);
        frequency_ = static_cast<T>(55);
        randomState_ = kDefaultRandomSeed;
        updateOscillatorFrequencies();
        updateToneFilter();
    }

    void noteOn(
        const std::uint8_t note,
        const T velocity,
        const bool legato) noexcept
    {
        currentNote_ = static_cast<std::uint8_t>(std::min<std::uint8_t>(note, 127));
        normalizedVelocity_ = std::clamp(velocity, static_cast<T>(0), static_cast<T>(1));
        const T randomGain = nextRandomBipolar() * humanize_ * static_cast<T>(0.035);
        const T randomCutoff = nextRandomBipolar() * humanize_ * static_cast<T>(420);
        humanizedGain_ = static_cast<T>(1) + randomGain;
        humanizedCutoffOffset_ = randomCutoff;
        velocityGain_ = velocityToGain(normalizedVelocity_) * humanizedGain_;
        attackNoiseEnvelope_ = fingerNoise_ * (static_cast<T>(0.15) + (normalizedVelocity_ * static_cast<T>(0.85)));
        frequency_ = midiNoteToFrequency(currentNote_);
        updateOscillatorFrequencies();
        updateToneFilter();

        if (!legato || amplitudeEnvelope_.getState() == cvdsp::modulation::ADSRState::Idle)
            amplitudeEnvelope_.noteOn();

        active_ = true;
    }

    void noteOff() noexcept
    {
        releaseNoiseEnvelope_ = std::max(
            releaseNoiseEnvelope_,
            fingerNoise_ * (static_cast<T>(0.08) + (normalizedVelocity_ * static_cast<T>(0.28))));
        amplitudeEnvelope_.noteOff();
    }

    [[nodiscard]] T processSample() noexcept
    {
        const T envelope = amplitudeEnvelope_.process();
        const T noise = processFingerNoise();

        if (amplitudeEnvelope_.getState() == cvdsp::modulation::ADSRState::Idle
            && std::abs(noise) <= static_cast<T>(1.0e-5))
        {
            active_ = false;
            return static_cast<T>(0);
        }

        const T main = mainOscillator_.process();
        const T sub = subOscillator_.process();
        const T mixed = (main * static_cast<T>(0.78)) + (sub * static_cast<T>(0.22));
        const T filtered = toneFilter_.process(mixed);
        const T enveloped = (filtered * envelope * velocityGain_) + noise;
        const T driven = applyDrive(enveloped);
        const T compressed = compressor_.process(driven);
        const T equalized = trebleEq_.process(midEq_.process(bassEq_.process(compressed)));
        return equalized * outputGain_;
    }

    void setTone(const T tone) noexcept
    {
        tone_ = std::clamp(tone, static_cast<T>(0), static_cast<T>(1));
        updateToneFilter();
    }

    void setAttackMs(const T attackMs) noexcept
    {
        attackMs_ = std::clamp(attackMs, static_cast<T>(0.5), static_cast<T>(40));
        amplitudeEnvelope_.setAttack(attackMs_);
    }

    void setVelocitySensitivity(const T sensitivity) noexcept
    {
        velocitySensitivity_ = std::clamp(sensitivity, static_cast<T>(0), static_cast<T>(1));
        velocityGain_ = velocityToGain(normalizedVelocity_);
        updateToneFilter();
    }

    void setCompression(const T amount) noexcept
    {
        compression_ = std::clamp(amount, static_cast<T>(0), static_cast<T>(1));
        updateCompressor();
    }

    void setDrive(const T amount) noexcept
    {
        drive_ = std::clamp(amount, static_cast<T>(0), static_cast<T>(1));
    }

    void setBassGainDb(const T gainDb) noexcept
    {
        bassGainDb_ = std::clamp(gainDb, static_cast<T>(-12), static_cast<T>(12));
        updateEq();
    }

    void setMidGainDb(const T gainDb) noexcept
    {
        midGainDb_ = std::clamp(gainDb, static_cast<T>(-12), static_cast<T>(12));
        updateEq();
    }

    void setTrebleGainDb(const T gainDb) noexcept
    {
        trebleGainDb_ = std::clamp(gainDb, static_cast<T>(-12), static_cast<T>(12));
        updateEq();
    }

    void setFingerNoise(const T amount) noexcept
    {
        fingerNoise_ = std::clamp(amount, static_cast<T>(0), static_cast<T>(1));
    }

    void setHumanize(const T amount) noexcept
    {
        humanize_ = std::clamp(amount, static_cast<T>(0), static_cast<T>(1));
    }

    void setOutputGainDb(const T gainDb) noexcept
    {
        outputGain_ = static_cast<T>(0.24)
            * std::pow(static_cast<T>(10), std::clamp(gainDb, static_cast<T>(-24), static_cast<T>(6)) / static_cast<T>(20));
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return active_;
    }

    [[nodiscard]] std::uint8_t getCurrentNote() const noexcept
    {
        return currentNote_;
    }

    [[nodiscard]] T getFrequency() const noexcept
    {
        return frequency_;
    }

private:
    [[nodiscard]] static T midiNoteToFrequency(const std::uint8_t note) noexcept
    {
        return static_cast<T>(440)
            * std::pow(static_cast<T>(2), (static_cast<T>(note) - static_cast<T>(69)) / static_cast<T>(12));
    }

    [[nodiscard]] T velocityToGain(const T velocity) const noexcept
    {
        const T shaped = static_cast<T>(0.18) + (std::sqrt(velocity) * static_cast<T>(0.82));
        return static_cast<T>(1) + ((shaped - static_cast<T>(1)) * velocitySensitivity_);
    }

    void updateToneFilter() noexcept
    {
        const T baseCutoff = static_cast<T>(280) + (tone_ * tone_ * static_cast<T>(3300));
        const T velocityLift = normalizedVelocity_ * velocitySensitivity_ * static_cast<T>(2200);
        toneFilter_.setCutoff(baseCutoff + velocityLift + humanizedCutoffOffset_);
    }

    void updateNoiseDecays() noexcept
    {
        attackNoiseDecay_ = std::exp(static_cast<T>(-1) / (std::max(sampleRate_, static_cast<T>(1)) * static_cast<T>(0.012)));
        releaseNoiseDecay_ = std::exp(static_cast<T>(-1) / (std::max(sampleRate_, static_cast<T>(1)) * static_cast<T>(0.035)));
    }

    [[nodiscard]] T processFingerNoise() noexcept
    {
        const T attackNoise = nextRandomBipolar() * attackNoiseEnvelope_ * static_cast<T>(0.08);
        const T releaseNoise = nextRandomBipolar() * releaseNoiseEnvelope_ * static_cast<T>(0.045);

        attackNoiseEnvelope_ *= attackNoiseDecay_;
        releaseNoiseEnvelope_ *= releaseNoiseDecay_;

        if (attackNoiseEnvelope_ < static_cast<T>(1.0e-5))
            attackNoiseEnvelope_ = static_cast<T>(0);
        if (releaseNoiseEnvelope_ < static_cast<T>(1.0e-5))
            releaseNoiseEnvelope_ = static_cast<T>(0);

        return attackNoise + releaseNoise;
    }

    [[nodiscard]] T nextRandomBipolar() noexcept
    {
        randomState_ = (randomState_ * 1664525u) + 1013904223u;
        const T normalized = static_cast<T>((randomState_ >> 8) & 0x00FFFFFFu)
            / static_cast<T>(0x00FFFFFFu);
        return (normalized * static_cast<T>(2)) - static_cast<T>(1);
    }

    void updateCompressor() noexcept
    {
        compressor_.setThresholdDB(static_cast<T>(0) - (compression_ * static_cast<T>(26)));
        compressor_.setRatio(static_cast<T>(1) + (compression_ * static_cast<T>(4)));
        compressor_.setMakeupGainDB(compression_ * static_cast<T>(3));
    }

    void configureEqTypes() noexcept
    {
        bassEq_.setType(cvdsp::filters::BiquadType::LowShelf);
        bassEq_.setFrequency(static_cast<T>(95));
        bassEq_.setQ(static_cast<T>(0.70710678));

        midEq_.setType(cvdsp::filters::BiquadType::PeakingEQ);
        midEq_.setFrequency(static_cast<T>(700));
        midEq_.setQ(static_cast<T>(0.9));

        trebleEq_.setType(cvdsp::filters::BiquadType::HighShelf);
        trebleEq_.setFrequency(static_cast<T>(2600));
        trebleEq_.setQ(static_cast<T>(0.70710678));
    }

    void updateEq() noexcept
    {
        bassEq_.setGainDB(bassGainDb_);
        bassEq_.updateCoefficients();
        midEq_.setGainDB(midGainDb_);
        midEq_.updateCoefficients();
        trebleEq_.setGainDB(trebleGainDb_);
        trebleEq_.updateCoefficients();
    }

    [[nodiscard]] T applyDrive(const T input) const noexcept
    {
        if (drive_ <= static_cast<T>(0.001))
            return input;

        const T driveGain = static_cast<T>(1) + (drive_ * static_cast<T>(5));
        const T shaped = cvdsp::saturation::Waveshaper<T>::process(
            input * driveGain,
            cvdsp::saturation::WaveshaperMode::Tanh);
        const T compensation = static_cast<T>(1) / (static_cast<T>(1) + (drive_ * static_cast<T>(1.8)));
        return shaped * compensation;
    }

    void updateOscillatorFrequencies() noexcept
    {
        mainOscillator_.setFrequency(frequency_);
        subOscillator_.setFrequency(frequency_ * static_cast<T>(0.5));
    }

    T sampleRate_ = static_cast<T>(44100);
    T frequency_ = static_cast<T>(55);
    T velocityGain_ = static_cast<T>(0);
    T normalizedVelocity_ = static_cast<T>(0);
    T tone_ = static_cast<T>(0.55);
    T velocitySensitivity_ = static_cast<T>(0.75);
    T attackMs_ = static_cast<T>(3);
    T fingerNoise_ = static_cast<T>(0.18);
    T humanize_ = static_cast<T>(0.18);
    T humanizedGain_ = static_cast<T>(1);
    T humanizedCutoffOffset_ = static_cast<T>(0);
    T attackNoiseEnvelope_ = static_cast<T>(0);
    T releaseNoiseEnvelope_ = static_cast<T>(0);
    T attackNoiseDecay_ = static_cast<T>(0.995);
    T releaseNoiseDecay_ = static_cast<T>(0.998);
    T compression_ = static_cast<T>(0.35);
    T drive_ = static_cast<T>(0.08);
    T bassGainDb_ = static_cast<T>(0);
    T midGainDb_ = static_cast<T>(0);
    T trebleGainDb_ = static_cast<T>(0);
    T outputGain_ = static_cast<T>(0.24);
    bool active_ = false;
    std::uint8_t currentNote_ = 0;
    static constexpr std::uint32_t kDefaultRandomSeed = 0xC0FFEEu;
    std::uint32_t randomState_ = kDefaultRandomSeed;

    cvdsp::modulation::Oscillator<T> mainOscillator_{};
    cvdsp::modulation::Oscillator<T> subOscillator_{};
    cvdsp::modulation::ADSR<T> amplitudeEnvelope_{};
    cvdsp::filters::StateVariableFilter<T> toneFilter_{};
    cvdsp::dynamics::Compressor<T> compressor_{};
    cvdsp::filters::Biquad<T> bassEq_{};
    cvdsp::filters::Biquad<T> midEq_{};
    cvdsp::filters::Biquad<T> trebleEq_{};
};

} // namespace cvdsp::synthesis::bass

#endif // CVDSP_SYNTHESIS_BASS_BASSFINGERVOICE_HPP
