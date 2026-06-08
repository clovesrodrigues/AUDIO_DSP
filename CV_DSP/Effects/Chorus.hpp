#ifndef CVDSP_EFFECTS_CHORUS_HPP
#define CVDSP_EFFECTS_CHORUS_HPP

/**
 * @file Chorus.hpp
 * @brief Mono Chorus Effect
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
 * @brief Mono Chorus.
 *
 * Parameters:
 *
 * Rate  (Hz)
 * Depth (0..1)
 * Mix   (0..1)
 *
 * Delay Modulation:
 *
 * Base Delay:
 * 15 ms
 *
 * Modulation:
 * ±10 ms
 */
template<typename T>
class Chorus
{
    static_assert(
        std::is_floating_point_v<T>,
        "Chorus requires floating point type");

public:

    constexpr Chorus() noexcept = default;

    /**
     * @brief Initialize chorus.
     *
     * @param sampleRate Processing sample rate.
     * @param maxDelayMs Maximum delay buffer size.
     */
    void prepare(
        T sampleRate,
        T maxDelayMs = static_cast<T>(50.0)) noexcept
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
     * @brief Reset effect state.
     */
    void reset() noexcept
    {
        delay_.reset();
        lfo_.reset();
    }

    /**
     * @brief Set modulation rate.
     *
     * Range:
     *
     * 0.01 Hz
     * to
     * 20 Hz
     */
    void setRate(
        T rateHz) noexcept
    {
        rateHz_ =
            std::clamp(
                rateHz,
                static_cast<T>(0.01),
                static_cast<T>(20.0));

        lfo_.setRate(rateHz_);
    }

    /**
     * @brief Set modulation depth.
     *
     * Range:
     *
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
     * @brief Set wet/dry mix.
     *
     * Range:
     *
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
                modulationDepthMs_
                *
                depth_
                *
                modulation
            );

        const T delayed =
            delay_.readInterpolated(
                delayMs);

        delay_.write(input);

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
        static_cast<T>(0.5);

    T depth_ =
        static_cast<T>(0.5);

    T mix_ =
        static_cast<T>(0.5);

    /**
     * Typical chorus values.
     */
    T baseDelayMs_ =
        static_cast<T>(15.0);

    T modulationDepthMs_ =
        static_cast<T>(10.0);
};

} // namespace cvdsp

#endif
