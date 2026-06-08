#ifndef CVDSP_EFFECTS_FLANGER_HPP
#define CVDSP_EFFECTS_FLANGER_HPP

/**
 * @file Flanger.hpp
 * @brief Feedback Flanger
 *
 * Dependencies:
 * - DelayLine.hpp
 * - LFO.hpp
 *
 * Header-only
 * C++20
 * Real-Time Safe
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "../Delay/DelayLine.hpp"
#include "../Modulation/LFO.hpp"

namespace cvdsp
{

/**
 * @brief Feedback Flanger.
 *
 * Signal Path:
 *
 * Input
 *   |
 *   +------------------------+
 *   |                        |
 *   v                        |
 * Delay Line                |
 *   |                        |
 *   v                        |
 * Delayed Signal -----------+
 *   |
 *   v
 * Wet/Dry Mix
 *   |
 * Output
 *
 * Parameters:
 *
 * Rate
 * Depth
 * Feedback
 * Mix
 */
template<typename T>
class Flanger
{
    static_assert(
        std::is_floating_point_v<T>,
        "Flanger requires floating point type");

public:

    constexpr Flanger() noexcept = default;

    /**
     * @brief Initialize flanger.
     *
     * @param sampleRate Processing sample rate.
     * @param maxDelayMs Delay buffer size.
     */
    void prepare(
        T sampleRate,
        T maxDelayMs = static_cast<T>(20.0)) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        delay_.prepare(
            sampleRate_,
            maxDelayMs);

        lfo_.prepare(
            sampleRate_,
            rateHz_,
            static_cast<T>(1),
            LFOWaveform::Sine);

        reset();
    }

    /**
     * @brief Reset internal state.
     */
    void reset() noexcept
    {
        delay_.reset();
        lfo_.reset();

        feedbackSample_ =
            static_cast<T>(0);
    }

    /**
     * @brief Set modulation rate.
     *
     * Range:
     * 0.01 Hz .. 20 Hz
     */
    void setRate(
        T rateHz) noexcept
    {
        rateHz_ =
            std::clamp(
                rateHz,
                static_cast<T>(0.01),
                static_cast<T>(20.0));

        lfo_.setRate(
            rateHz_);
    }

    /**
     * @brief Set modulation depth.
     *
     * Range:
     * 0.0 .. 1.0
     */
    void setDepth(
        T depth) noexcept
    {
        depth_ =
            std::clamp(
                depth,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Set feedback amount.
     *
     * Range:
     * -0.99 .. +0.99
     */
    void setFeedback(
        T feedback) noexcept
    {
        feedback_ =
            std::clamp(
                feedback,
                static_cast<T>(-0.99),
                static_cast<T>(0.99));
    }

    /**
     * @brief Set wet/dry mix.
     *
     * Range:
     * 0.0 .. 1.0
     */
    void setMix(
        T mix) noexcept
    {
        mix_ =
            std::clamp(
                mix,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Process one sample.
     */
    inline T process(
        T input) noexcept
    {
        const T modulation =
            lfo_.process();

        const T delayMs =
            baseDelayMs_
            +
            (
                maxModulationMs_
                *
                depth_
                *
                modulation
            );

        const T delayed =
            delay_.readInterpolated(
                delayMs);

        const T delayInput =
            input
            +
            (
                delayed
                *
                feedback_
            );

        delay_.write(
            delayInput);

        feedbackSample_ =
            delayed;

        const T dry =
            input;

        const T wet =
            delayed;

        return
            (
                dry
                *
                (
                    static_cast<T>(1)
                    -
                    mix_
                )
            )
            +
            (
                wet
                *
                mix_
            );
    }

private:

    delay::DelayLine<T> delay_;

    LFO<T> lfo_;

    T sampleRate_ =
        static_cast<T>(44100);

    T rateHz_ =
        static_cast<T>(0.25);

    T depth_ =
        static_cast<T>(0.5);

    T feedback_ =
        static_cast<T>(0.5);

    T mix_ =
        static_cast<T>(0.5);

    /**
     * Typical flanger delay.
     *
     * Smaller than chorus.
     */
    T baseDelayMs_ =
        static_cast<T>(2.0);

    /**
     * Maximum modulation range.
     */
    T maxModulationMs_ =
        static_cast<T>(3.0);

    T feedbackSample_ =
        static_cast<T>(0);
};

} // namespace cvdsp

#endif
