#ifndef CVDSP_MODULATION_ADSR_HPP
#define CVDSP_MODULATION_ADSR_HPP

/**
 * @file ADSR.hpp
 * @brief ADSR Envelope Generator
 *
 * Header-only
 * C++20
 * Real-Time Safe
 * No dynamic allocation
 */

#include <algorithm>
#include <type_traits>

namespace cvdsp::modulation
{

/**
 * @brief ADSR envelope states.
 */
enum class ADSRState
{
    Idle,
    Attack,
    Decay,
    Sustain,
    Release
};

/**
 * @brief ADSR Envelope Generator.
 *
 * Output Range:
 *
 * 0.0 -> 1.0
 *
 * Stages:
 *
 * Attack
 * Decay
 * Sustain
 * Release
 *
 * Real-Time Safe
 * Header Only
 */
template<typename T>
class ADSR
{
    static_assert(
        std::is_floating_point_v<T>,
        "ADSR requires floating point type");

public:

    constexpr ADSR() noexcept = default;

    /**
     * @brief Prepare envelope.
     *
     * @param sampleRate Processing sample rate.
     * @param attackMs Attack time.
     * @param decayMs Decay time.
     * @param sustain Sustain level.
     * @param releaseMs Release time.
     */
    void prepare(
        T sampleRate,
        T attackMs,
        T decayMs,
        T sustain,
        T releaseMs) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        attackMs_ =
            std::max(
                attackMs,
                static_cast<T>(0.001));

        decayMs_ =
            std::max(
                decayMs,
                static_cast<T>(0.001));

        sustainLevel_ =
            std::clamp(
                sustain,
                static_cast<T>(0),
                static_cast<T>(1));

        releaseMs_ =
            std::max(
                releaseMs,
                static_cast<T>(0.001));

        updateCoefficients();

        reset();
    }

    /**
     * @brief Reset envelope state.
     */
    void reset() noexcept
    {
        value_ =
            static_cast<T>(0);

        releaseStartLevel_ =
            static_cast<T>(0);

        state_ =
            ADSRState::Idle;
    }

    /**
     * @brief Start envelope.
     */
    void noteOn() noexcept
    {
        state_ =
            ADSRState::Attack;
    }

    /**
     * @brief Release envelope.
     */
    void noteOff() noexcept
    {
        releaseStartLevel_ =
            value_;

        state_ =
            ADSRState::Release;
    }

    /**
     * @brief Set attack time.
     */
    void setAttack(
        T attackMs) noexcept
    {
        attackMs_ =
            std::max(
                attackMs,
                static_cast<T>(0.001));

        updateAttack();
    }

    /**
     * @brief Set decay time.
     */
    void setDecay(
        T decayMs) noexcept
    {
        decayMs_ =
            std::max(
                decayMs,
                static_cast<T>(0.001));

        updateDecay();
    }

    /**
     * @brief Set sustain level.
     */
    void setSustain(
        T sustain) noexcept
    {
        sustainLevel_ =
            std::clamp(
                sustain,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Set release time.
     */
    void setRelease(
        T releaseMs) noexcept
    {
        releaseMs_ =
            std::max(
                releaseMs,
                static_cast<T>(0.001));

        updateRelease();
    }

    /**
     * @brief Process one sample.
     *
     * @return Envelope value.
     */
    inline T process() noexcept
    {
        switch (state_)
        {
            case ADSRState::Idle:
            {
                value_ =
                    static_cast<T>(0);
                break;
            }

            case ADSRState::Attack:
            {
                value_ += attackIncrement_;

                if (value_ >= static_cast<T>(1))
                {
                    value_ =
                        static_cast<T>(1);

                    state_ =
                        ADSRState::Decay;
                }

                break;
            }

            case ADSRState::Decay:
            {
                value_ -= decayIncrement_;

                if (value_ <= sustainLevel_)
                {
                    value_ =
                        sustainLevel_;

                    state_ =
                        ADSRState::Sustain;
                }

                break;
            }

            case ADSRState::Sustain:
            {
                value_ =
                    sustainLevel_;
                break;
            }

            case ADSRState::Release:
            {
                value_ -= releaseIncrement_;

                if (value_ <= static_cast<T>(0))
                {
                    value_ =
                        static_cast<T>(0);

                    state_ =
                        ADSRState::Idle;
                }

                break;
            }

            default:
            {
                value_ =
                    static_cast<T>(0);

                state_ =
                    ADSRState::Idle;
                break;
            }
        }

        return value_;
    }

    /**
     * @brief Current state.
     */
    [[nodiscard]]
    ADSRState getState() const noexcept
    {
        return state_;
    }

    /**
     * @brief Current envelope value.
     */
    [[nodiscard]]
    T getValue() const noexcept
    {
        return value_;
    }

private:

    void updateCoefficients() noexcept
    {
        updateAttack();
        updateDecay();
        updateRelease();
    }

    void updateAttack() noexcept
    {
        const T attackSamples =
            attackMs_
            *
            static_cast<T>(0.001)
            *
            sampleRate_;

        attackIncrement_ =
            static_cast<T>(1)
            /
            std::max(
                attackSamples,
                static_cast<T>(1));
    }

    void updateDecay() noexcept
    {
        const T decaySamples =
            decayMs_
            *
            static_cast<T>(0.001)
            *
            sampleRate_;

        decayIncrement_ =
            (
                static_cast<T>(1)
                -
                sustainLevel_
            )
            /
            std::max(
                decaySamples,
                static_cast<T>(1));
    }

    void updateRelease() noexcept
    {
        const T releaseSamples =
            releaseMs_
            *
            static_cast<T>(0.001)
            *
            sampleRate_;

        releaseIncrement_ =
            static_cast<T>(1)
            /
            std::max(
                releaseSamples,
                static_cast<T>(1));
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T attackMs_ =
        static_cast<T>(10);

    T decayMs_ =
        static_cast<T>(100);

    T sustainLevel_ =
        static_cast<T>(0.7);

    T releaseMs_ =
        static_cast<T>(250);

    T attackIncrement_ =
        static_cast<T>(0);

    T decayIncrement_ =
        static_cast<T>(0);

    T releaseIncrement_ =
        static_cast<T>(0);

    T value_ =
        static_cast<T>(0);

    T releaseStartLevel_ =
        static_cast<T>(0);

    ADSRState state_ =
        ADSRState::Idle;
};

} // namespace cvdsp::modulation

namespace cvdsp
{
template<typename T>
using ADSR = modulation::ADSR<T>;

using ADSRState = modulation::ADSRState;
} // namespace cvdsp

#endif
