#ifndef CVDSP_CONTROL_EXPRESSIONENGINE_HPP
#define CVDSP_CONTROL_EXPRESSIONENGINE_HPP

/**
 * @file ExpressionEngine.hpp
 * @brief Real-time-safe musical expression generator for pedals and filters.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../Core/ParameterSmoother.hpp"
#include "../Core/ProcessContext.hpp"
#include "../Dynamics/EnvelopeFollower.hpp"

namespace cvdsp::control
{

/**
 * @brief High-level expression behavior selected from audio/transport features.
 */
enum class ExpressionState : std::uint32_t
{
    Idle = 0,
    AttackAccent,
    Rhythmic,
    Vocal
};

/**
 * @brief Host-synced rhythmic subdivision used by the expression generator.
 */
enum class ExpressionSubdivision : std::uint32_t
{
    Quarter = 0,
    Eighth,
    Sixteenth,
    ThirtySecond
};

/**
 * @brief Extracts simple guitar-performance features and generates expression p in [0, 1].
 *
 * Version 1 intentionally avoids pitch tracking and neural inference. It uses an
 * envelope follower, a transient/density tracker, host BPM/PPQ and a small state
 * machine to generate deterministic, allocation-free control values suitable for
 * driving WahWahDSP or other expression-controlled processors.
 */
template<typename T = float>
class ExpressionEngine
{
    static_assert(std::is_floating_point_v<T>, "ExpressionEngine requires a floating point type");

public:
    using value_type = T;
    using size_type = std::size_t;

    constexpr ExpressionEngine() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(44100);

        envelope_.prepare(sampleRate_);
        envelope_.setMode(dynamics::EnvelopeMode::Peak);
        envelope_.setAttackMs(static_cast<T>(2));
        envelope_.setReleaseMs(static_cast<T>(180));

        expressionSmoother_.prepare(sampleRate_, static_cast<T>(0.025));
        updateDetectorCoefficients();
        reset();
    }

    void reset() noexcept
    {
        envelope_.reset();
        fastEnergy_ = static_cast<T>(0);
        slowEnergy_ = static_cast<T>(0);
        envelopeValue_ = static_cast<T>(0);
        targetExpression_ = idlePosition_;
        expressionSmoother_.reset(idlePosition_);
        state_ = ExpressionState::Idle;
        subdivision_ = ExpressionSubdivision::Sixteenth;
        sampleCounter_ = 0;
        lastTransientSample_ = 0;
        samplesSinceTransient_ = maxIntervalSamples();
        noteAgeSamples_ = 0;
        averageTransientIntervalSamples_ = highDensitySamples();
        transientCount_ = 0;
        vocalPhase_ = static_cast<T>(0);
        lastPPQPosition_ = static_cast<T>(0);
        bpm_ = static_cast<T>(120);
    }

    void setIdlePosition(T normalized) noexcept { idlePosition_ = clamp01(normalized); }
    void setSensitivity(T normalized) noexcept { sensitivity_ = clamp01(normalized); }
    void setTransientSensitivity(T normalized) noexcept { transientSensitivity_ = clamp01(normalized); }
    void setRhythmicDepth(T normalized) noexcept { rhythmicDepth_ = clamp01(normalized); }
    void setVocalDepth(T normalized) noexcept { vocalDepth_ = clamp01(normalized); }

    void setSubdivision(ExpressionSubdivision subdivision) noexcept
    {
        subdivision_ = subdivision;
    }

    void updateFeatures(const T* audioBlock, size_type numSamples, double bpm, double ppqPosition) noexcept
    {
        if (audioBlock == nullptr || numSamples == 0)
        {
            selectStateAndTarget(static_cast<T>(bpm), static_cast<T>(ppqPosition), 0);
            return;
        }

        const T safeBpm = sanitizeBpm(static_cast<T>(bpm));
        const T ppq = static_cast<T>(ppqPosition);
        bpm_ = safeBpm;
        lastPPQPosition_ = ppq;

        size_type blockTransients = 0;
        for (size_type i = 0; i < numSamples; ++i)
        {
            processAnalysisSample(audioBlock[i], blockTransients);
            ++sampleCounter_;
        }

        selectStateAndTarget(safeBpm, ppq, numSamples);
    }

    void updateFeatures(const T* audioBlock, size_type numSamples, const ProcessContext<T>& context) noexcept
    {
        updateFeatures(audioBlock, numSamples, static_cast<double>(context.tempo), static_cast<double>(context.ppqPosition));
    }

    [[nodiscard]] T process() noexcept
    {
        expressionSmoother_.setTarget(targetExpression_);
        return expressionSmoother_.process();
    }

    [[nodiscard]] T getTargetExpression() const noexcept { return targetExpression_; }
    [[nodiscard]] T getCurrentExpression() const noexcept { return expressionSmoother_.getCurrentValue(); }
    [[nodiscard]] T getEnvelope() const noexcept { return envelopeValue_; }
    [[nodiscard]] ExpressionState getState() const noexcept { return state_; }
    [[nodiscard]] ExpressionSubdivision getSubdivision() const noexcept { return subdivision_; }
    [[nodiscard]] T getBpm() const noexcept { return bpm_; }
    [[nodiscard]] T getAverageTransientIntervalMs() const noexcept
    {
        return (averageTransientIntervalSamples_ / sampleRate_) * static_cast<T>(1000);
    }

private:
    void processAnalysisSample(T input, size_type& blockTransients) noexcept
    {
        const T x = std::abs(input);
        envelopeValue_ = envelope_.process(input);

        fastEnergy_ += fastCoeff_ * (x - fastEnergy_);
        slowEnergy_ += slowCoeff_ * (x - slowEnergy_);

        ++samplesSinceTransient_;

        if (envelopeValue_ > silenceThreshold())
            ++noteAgeSamples_;
        else
            noteAgeSamples_ = 0;

        const T sensitivityScale = static_cast<T>(1.8) - transientSensitivity_;
        const T transientThreshold = std::max(static_cast<T>(0.002), slowEnergy_ * sensitivityScale);
        const bool cooldownElapsed = samplesSinceTransient_ >= transientCooldownSamples();
        const bool transient = cooldownElapsed && fastEnergy_ > transientThreshold && fastEnergy_ > silenceThreshold();

        if (transient)
        {
            const T interval = static_cast<T>(samplesSinceTransient_);
            const T validInterval = std::min(interval, maxIntervalSamples());
            averageTransientIntervalSamples_ = averageTransientIntervalSamples_ * static_cast<T>(0.75)
                                             + validInterval * static_cast<T>(0.25);
            samplesSinceTransient_ = 0;
            lastTransientSample_ = sampleCounter_;
            ++transientCount_;
            ++blockTransients;
            noteAgeSamples_ = 0;
        }
    }

    void selectStateAndTarget(T bpm, T ppqPosition, size_type numSamples) noexcept
    {
        advanceVocalPhase(numSamples);

        const bool silent = envelopeValue_ < silenceThreshold();
        const bool recentTransient = samplesSinceTransient_ < attackWindowSamples();
        const bool highDensity = averageTransientIntervalSamples_ < highDensitySamples()
                              && samplesSinceTransient_ < maxIntervalSamples();
        const bool veryHighDensity = averageTransientIntervalSamples_ < veryHighDensitySamples()
                                  && samplesSinceTransient_ < maxIntervalSamples();
        const bool sustained = noteAgeSamples_ > vocalMinSamples()
                            && samplesSinceTransient_ > highDensitySamples()
                            && envelopeValue_ > silenceThreshold() * static_cast<T>(4);

        if (silent)
        {
            state_ = ExpressionState::Idle;
            targetExpression_ = idlePosition_;
        }
        else if (veryHighDensity)
        {
            state_ = ExpressionState::Rhythmic;
            targetExpression_ = staticExpressionForFastPicking();
        }
        else if (highDensity)
        {
            state_ = ExpressionState::Rhythmic;
            targetExpression_ = rhythmicExpression(bpm, ppqPosition);
        }
        else if (sustained)
        {
            state_ = ExpressionState::Vocal;
            targetExpression_ = vocalExpression();
        }
        else if (recentTransient)
        {
            state_ = ExpressionState::AttackAccent;
            targetExpression_ = attackExpression();
        }
        else
        {
            state_ = ExpressionState::Idle;
            targetExpression_ = idlePosition_;
        }

        targetExpression_ = clamp01(targetExpression_);
        expressionSmoother_.setTarget(targetExpression_);
    }

    [[nodiscard]] T attackExpression() const noexcept
    {
        const T age = static_cast<T>(samplesSinceTransient_);
        const T tauSamples = sampleRate_ * static_cast<T>(0.060);
        const T fall = std::exp(-age / std::max(tauSamples, static_cast<T>(1)));
        return static_cast<T>(0.2) + static_cast<T>(0.7) * fall;
    }

    [[nodiscard]] T rhythmicExpression(T, T ppqPosition) const noexcept
    {
        const T phase = normalizedGridPhase(ppqPosition);
        const T saw = smoothSaw(phase);
        const T low = static_cast<T>(0.3);
        const T high = static_cast<T>(0.3) + static_cast<T>(0.4) * rhythmicDepth_;
        return low + (high - low) * saw;
    }

    [[nodiscard]] T staticExpressionForFastPicking() const noexcept
    {
        return static_cast<T>(0.65);
    }

    [[nodiscard]] T vocalExpression() const noexcept
    {
        constexpr T kTwoPi = static_cast<T>(6.2831853071795864769);
        const T depth = static_cast<T>(0.6) * vocalDepth_ * std::clamp(envelopeValue_ * static_cast<T>(8), static_cast<T>(0), static_cast<T>(1));
        const T center = static_cast<T>(0.5);
        const T sine = static_cast<T>(0.5) + static_cast<T>(0.5) * std::sin(vocalPhase_ * kTwoPi);
        return center - depth * static_cast<T>(0.5) + depth * sine;
    }

    void advanceVocalPhase(size_type numSamples) noexcept
    {
        const T seconds = static_cast<T>(numSamples) / sampleRate_;
        vocalPhase_ += seconds * static_cast<T>(0.18);
        while (vocalPhase_ >= static_cast<T>(1))
            vocalPhase_ -= static_cast<T>(1);
    }

    [[nodiscard]] T normalizedGridPhase(T ppqPosition) const noexcept
    {
        const T multiplier = subdivisionMultiplier();
        const T position = ppqPosition * multiplier;
        return position - std::floor(position);
    }

    [[nodiscard]] T subdivisionMultiplier() const noexcept
    {
        switch (subdivision_)
        {
            case ExpressionSubdivision::Quarter: return static_cast<T>(1);
            case ExpressionSubdivision::Eighth: return static_cast<T>(2);
            case ExpressionSubdivision::Sixteenth: return static_cast<T>(4);
            case ExpressionSubdivision::ThirtySecond: return static_cast<T>(8);
            default: return static_cast<T>(4);
        }
    }

    [[nodiscard]] static T smoothSaw(T phase) noexcept
    {
        const T p = clamp01(phase);
        return p * p * (static_cast<T>(3) - static_cast<T>(2) * p);
    }

    [[nodiscard]] static T sanitizeBpm(T bpm) noexcept
    {
        return std::clamp(bpm, static_cast<T>(20), static_cast<T>(320));
    }

    [[nodiscard]] static constexpr T clamp01(T value) noexcept
    {
        return value < static_cast<T>(0) ? static_cast<T>(0) : (value > static_cast<T>(1) ? static_cast<T>(1) : value);
    }

    void updateDetectorCoefficients() noexcept
    {
        fastCoeff_ = onePoleCoeff(static_cast<T>(0.003));
        slowCoeff_ = onePoleCoeff(static_cast<T>(0.040));
    }

    [[nodiscard]] T onePoleCoeff(T seconds) const noexcept
    {
        return static_cast<T>(1) - std::exp(static_cast<T>(-1) / (seconds * sampleRate_));
    }

    [[nodiscard]] T silenceThreshold() const noexcept
    {
        return static_cast<T>(0.0015) * (static_cast<T>(1.25) - sensitivity_ * static_cast<T>(0.75));
    }

    [[nodiscard]] T transientCooldownSamples() const noexcept { return sampleRate_ * static_cast<T>(0.035); }
    [[nodiscard]] T attackWindowSamples() const noexcept { return sampleRate_ * static_cast<T>(0.120); }
    [[nodiscard]] T highDensitySamples() const noexcept { return sampleRate_ * static_cast<T>(0.150); }
    [[nodiscard]] T veryHighDensitySamples() const noexcept { return sampleRate_ * static_cast<T>(0.075); }
    [[nodiscard]] T vocalMinSamples() const noexcept { return sampleRate_ * static_cast<T>(1.000); }
    [[nodiscard]] T maxIntervalSamples() const noexcept { return sampleRate_ * static_cast<T>(1.500); }

    T sampleRate_ { static_cast<T>(44100) };
    T bpm_ { static_cast<T>(120) };
    T lastPPQPosition_ { static_cast<T>(0) };

    T idlePosition_ { static_cast<T>(0.4) };
    T sensitivity_ { static_cast<T>(0.55) };
    T transientSensitivity_ { static_cast<T>(0.55) };
    T rhythmicDepth_ { static_cast<T>(1) };
    T vocalDepth_ { static_cast<T>(1) };

    T fastEnergy_ { static_cast<T>(0) };
    T slowEnergy_ { static_cast<T>(0) };
    T fastCoeff_ { static_cast<T>(1) };
    T slowCoeff_ { static_cast<T>(1) };
    T envelopeValue_ { static_cast<T>(0) };
    T targetExpression_ { static_cast<T>(0.4) };
    T vocalPhase_ { static_cast<T>(0) };
    T averageTransientIntervalSamples_ { static_cast<T>(0) };

    std::uint64_t sampleCounter_ { 0 };
    std::uint64_t lastTransientSample_ { 0 };
    T samplesSinceTransient_ { static_cast<T>(0) };
    T noteAgeSamples_ { static_cast<T>(0) };
    std::uint32_t transientCount_ { 0 };

    ExpressionState state_ { ExpressionState::Idle };
    ExpressionSubdivision subdivision_ { ExpressionSubdivision::Sixteenth };

    dynamics::EnvelopeFollower<T> envelope_ {};
    OnePoleSmoother<T> expressionSmoother_ {};
};

} // namespace cvdsp::control

#endif // CVDSP_CONTROL_EXPRESSIONENGINE_HPP
