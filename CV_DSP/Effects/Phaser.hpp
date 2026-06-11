#ifndef CVDSP_EFFECTS_PHASER_HPP
#define CVDSP_EFFECTS_PHASER_HPP

/**
 * @file Phaser.hpp
 * @brief LFO-modulated first-order all-pass phaser.
 *
 * Header-only, C++20 and real-time safe.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "../Modulation/LFO.hpp"

namespace cvdsp
{

/**
 * @brief Classic mono phaser built from cascaded first-order all-pass stages.
 *
 * The wet path runs through 2..8 first-order all-pass filters. The all-pass
 * coefficient is modulated by an LFO over a logarithmic frequency range and the
 * wet path is summed 50/50 with dry when mix is fully wet, creating moving phase
 * cancellation notches without oversampling or nonlinear processing.
 */
template<typename T>
class Phaser
{
    static_assert(std::is_floating_point_v<T>, "Phaser requires floating point type");

public:
    constexpr Phaser() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = std::max(sampleRate, static_cast<T>(1));
        lfo_.prepare(sampleRate_, rateHz_, static_cast<T>(1), LFOWaveform::Sine);
        updateAllPassCoefficient(centerFrequency());
        reset();
    }

    void reset() noexcept
    {
        for (auto& stage : stages_)
            stage.reset();
        lfo_.reset();
        feedbackSample_ = static_cast<T>(0);
    }

    void setRate(T rateHz) noexcept
    {
        rateHz_ = std::clamp(rateHz, static_cast<T>(0.01), static_cast<T>(20));
        lfo_.setRate(rateHz_);
    }

    void setDepth(T normalized) noexcept
    {
        depth_ = std::clamp(normalized, static_cast<T>(0), static_cast<T>(1));
    }

    void setFeedback(T feedback) noexcept
    {
        feedback_ = std::clamp(feedback, static_cast<T>(-0.95), static_cast<T>(0.95));
    }

    void setMix(T normalized) noexcept
    {
        mix_ = std::clamp(normalized, static_cast<T>(0), static_cast<T>(1));
    }

    void setFrequencyRange(T minHz, T maxHz) noexcept
    {
        minFrequencyHz_ = clampFrequency(minHz);
        maxFrequencyHz_ = clampFrequency(maxHz);
        if (maxFrequencyHz_ < minFrequencyHz_)
            std::swap(minFrequencyHz_, maxFrequencyHz_);
    }

    void setStages(std::size_t stages) noexcept
    {
        stages = std::clamp<std::size_t>(stages, 2u, kMaxStages);
        if ((stages % 2u) != 0u)
            ++stages;
        activeStages_ = std::min(stages, kMaxStages);
    }

    void setWaveform(LFOWaveform waveform) noexcept
    {
        lfo_.setWaveform(waveform);
    }

    [[nodiscard]] inline T process(T input) noexcept
    {
        const T lfoValue = lfo_.process();
        const T normalized = std::clamp(static_cast<T>(0.5) + static_cast<T>(0.5) * depth_ * lfoValue,
                                        static_cast<T>(0),
                                        static_cast<T>(1));
        const T frequencyHz = mapFrequency(normalized);
        updateAllPassCoefficient(frequencyHz);

        T wet = input + feedbackSample_ * feedback_;
        for (std::size_t i = 0; i < activeStages_; ++i)
            wet = stages_[i].process(wet);

        feedbackSample_ = wet;

        const T phaserWet = (input + wet) * static_cast<T>(0.5);
        return input * (static_cast<T>(1) - mix_) + phaserWet * mix_;
    }

    [[nodiscard]] T getRate() const noexcept { return rateHz_; }
    [[nodiscard]] T getDepth() const noexcept { return depth_; }
    [[nodiscard]] T getFeedback() const noexcept { return feedback_; }
    [[nodiscard]] T getMix() const noexcept { return mix_; }
    [[nodiscard]] T getMinFrequencyHz() const noexcept { return minFrequencyHz_; }
    [[nodiscard]] T getMaxFrequencyHz() const noexcept { return maxFrequencyHz_; }
    [[nodiscard]] std::size_t getStages() const noexcept { return activeStages_; }

private:
    class FirstOrderAllPass
    {
    public:
        void reset() noexcept
        {
            x1_ = static_cast<T>(0);
            y1_ = static_cast<T>(0);
        }

        void setCoefficient(T coefficient) noexcept
        {
            coefficient_ = std::clamp(coefficient, static_cast<T>(-0.999), static_cast<T>(0.999));
        }

        [[nodiscard]] inline T process(T input) noexcept
        {
            const T output = coefficient_ * input + x1_ - coefficient_ * y1_;
            x1_ = input;
            y1_ = output;
            return output;
        }

    private:
        T coefficient_ { static_cast<T>(0) };
        T x1_ { static_cast<T>(0) };
        T y1_ { static_cast<T>(0) };
    };

    [[nodiscard]] T clampFrequency(T frequencyHz) const noexcept
    {
        const T maxSafe = std::max(static_cast<T>(20), sampleRate_ * static_cast<T>(0.45));
        return std::clamp(frequencyHz, static_cast<T>(20), std::min(static_cast<T>(8000), maxSafe));
    }

    [[nodiscard]] T centerFrequency() const noexcept
    {
        return std::sqrt(std::max(minFrequencyHz_, static_cast<T>(1)) * std::max(maxFrequencyHz_, static_cast<T>(1)));
    }

    [[nodiscard]] T mapFrequency(T normalized) const noexcept
    {
        const T minHz = std::max(minFrequencyHz_, static_cast<T>(20));
        const T maxHz = std::max(maxFrequencyHz_, minHz);
        const T ratio = maxHz / minHz;
        return minHz * std::pow(ratio, std::clamp(normalized, static_cast<T>(0), static_cast<T>(1)));
    }

    void updateAllPassCoefficient(T frequencyHz) noexcept
    {
        constexpr T kPi = static_cast<T>(3.14159265358979323846);
        const T safeFrequency = clampFrequency(frequencyHz);
        const T t = std::tan(kPi * safeFrequency / sampleRate_);
        const T coefficient = (t - static_cast<T>(1)) / (t + static_cast<T>(1));
        for (auto& stage : stages_)
            stage.setCoefficient(coefficient);
    }

    static constexpr std::size_t kMaxStages = 8;

    std::array<FirstOrderAllPass, kMaxStages> stages_ {};
    LFO<T> lfo_ {};

    T sampleRate_ { static_cast<T>(44100) };
    T rateHz_ { static_cast<T>(0.35) };
    T depth_ { static_cast<T>(0.85) };
    T feedback_ { static_cast<T>(0.25) };
    T mix_ { static_cast<T>(1) };
    T minFrequencyHz_ { static_cast<T>(250) };
    T maxFrequencyHz_ { static_cast<T>(1600) };
    T feedbackSample_ { static_cast<T>(0) };
    std::size_t activeStages_ { 4 };
};

} // namespace cvdsp

#endif // CVDSP_EFFECTS_PHASER_HPP
