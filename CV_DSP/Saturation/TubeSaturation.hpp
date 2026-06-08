#ifndef CVDSP_SATURATION_TUBESATURATION_HPP
#define CVDSP_SATURATION_TUBESATURATION_HPP

/**
 * @file TubeSaturation.hpp
 * @brief Tube Style Saturation
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - Drive
 * - Bias
 * - Output Gain
 * - Even Harmonic Generation
 * - Asymmetrical Saturation
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace cvdsp::saturation
{

/**
 * @brief Tube style saturation processor.
 *
 * Inspired by triode preamp stages.
 *
 * Produces:
 *
 * - Soft compression
 * - Even harmonics
 * - Asymmetrical distortion
 *
 * Real-Time Safe
 */
template<typename T>
class TubeSaturation
{
    static_assert(
        std::is_floating_point_v<T>,
        "TubeSaturation requires floating point type");

public:

    constexpr TubeSaturation() noexcept = default;

    /**
     * @brief Initialize processor.
     *
     * Present for API consistency.
     */
    void prepare(
        T sampleRate) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        reset();
    }

    /**
     * @brief Reset internal state.
     */
    void reset() noexcept
    {
        previousOutput_ =
            static_cast<T>(0);
    }

    /**
     * @brief Set drive amount.
     *
     * Range:
     *
     * 1.0 .. 50.0
     */
    void setDrive(
        T drive) noexcept
    {
        drive_ =
            std::clamp(
                drive,
                static_cast<T>(1),
                static_cast<T>(50));
    }

    /**
     * @brief Set DC bias.
     *
     * Range:
     *
     * -1.0 .. +1.0
     */
    void setBias(
        T bias) noexcept
    {
        bias_ =
            std::clamp(
                bias,
                static_cast<T>(-1),
                static_cast<T>(1));
    }

    /**
     * @brief Set output gain.
     *
     * Linear gain.
     *
     * Range:
     *
     * 0.0 .. 10.0
     */
    void setOutputGain(
        T gain) noexcept
    {
        outputGain_ =
            std::clamp(
                gain,
                static_cast<T>(0),
                static_cast<T>(10));
    }

    /**
     * @brief Process one sample.
     */
    inline T process(
        T input) noexcept
    {
        T x =
            input
            *
            drive_;

        /**
         * Bias shift.
         */
        x += bias_;

        /**
         * Asymmetrical triode response.
         *
         * Positive half:
         * softer
         *
         * Negative half:
         * stronger compression
         */
        T y;

        if (x >= static_cast<T>(0))
        {
            y =
                positiveShape(x);
        }
        else
        {
            y =
                negativeShape(x);
        }

        /**
         * Remove part of the bias.
         */
        y -=
            bias_
            *
            static_cast<T>(0.5);

        y *= outputGain_;

        previousOutput_ = y;

        return y;
    }

private:

    /**
     * @brief Positive triode curve.
     */
    static inline T positiveShape(
        T x) noexcept
    {
        return
            static_cast<T>(1)
            -
            std::exp(
                -x);
    }

    /**
     * @brief Negative triode curve.
     */
    static inline T negativeShape(
        T x) noexcept
    {
        return
            -(
                static_cast<T>(1)
                -
                std::exp(
                    static_cast<T>(1.5)
                    *
                    x));
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T outputGain_ =
        static_cast<T>(1);

    T previousOutput_ =
        static_cast<T>(0);
};

} // namespace cvdsp::saturation

namespace cvdsp
{
template<typename T>
using TubeSaturation = saturation::TubeSaturation<T>;
} // namespace cvdsp

#endif
