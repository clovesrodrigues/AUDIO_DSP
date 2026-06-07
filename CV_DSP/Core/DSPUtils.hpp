#ifndef CVDSP_CORE_DSPUTILS_HPP
#define CVDSP_CORE_DSPUTILS_HPP

/**
 * @file DSPUtils.hpp
 * @brief Common DSP Utility Functions
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Centralized utility functions for the entire CV_DSP library.
 *
 * All generic reusable DSP math should live here.
 *
 * Dependencies:
 * - Core/Constants.hpp
 * - Core/Types.hpp
 * - Math/FastMath.hpp
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include "Constants.hpp"
#include "Types.hpp"
#include "../Math/FastMath.hpp"

namespace cvdsp
{

/**
 * @brief Utility DSP functions.
 *
 * Static-only utility class.
 */
class DSPUtils
{
public:

    DSPUtils() = delete;

    DSPUtils(const DSPUtils&) = delete;
    DSPUtils& operator=(const DSPUtils&) = delete;

    /**
     * @brief Convert dB to linear gain.
     *
     * gain = 10^(dB/20)
     */
    template<typename T>
    [[nodiscard]]
    static inline T dbToGain(
        T dB) noexcept
    {
        static_assert(
            std::is_floating_point_v<T>);

        return std::pow(
            static_cast<T>(10),
            dB / static_cast<T>(20));
    }

    /**
     * @brief Convert linear gain to dB.
     *
     * dB = 20*log10(gain)
     */
    template<typename T>
    [[nodiscard]]
    static inline T gainToDb(
        T gain) noexcept
    {
        static_assert(
            std::is_floating_point_v<T>);

        constexpr T minGain =
            static_cast<T>(1e-20);

        gain =
            std::max(
                gain,
                minGain);

        return
            static_cast<T>(20)
            *
            std::log10(gain);
    }

    /**
     * @brief Clamp value.
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T clamp(
        T value,
        T minimum,
        T maximum) noexcept
    {
        return
            value < minimum
            ? minimum
            : (
                value > maximum
                ? maximum
                : value
            );
    }

    /**
     * @brief Linear interpolation.
     *
     * t = 0 -> a
     * t = 1 -> b
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T lerp(
        T a,
        T b,
        T t) noexcept
    {
        return
            a
            +
            (
                b - a
            )
            *
            t;
    }

    /**
     * @brief Linear range mapping.
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T mapLinear(
        T value,
        T inMin,
        T inMax,
        T outMin,
        T outMax) noexcept
    {
        if (inMax == inMin)
        {
            return outMin;
        }

        const T normalized =
            (
                value - inMin
            )
            /
            (
                inMax - inMin
            );

        return
            outMin
            +
            normalized
            *
            (
                outMax - outMin
            );
    }

    /**
     * @brief Logarithmic mapping.
     *
     * Useful for:
     * - Frequency controls
     * - Time controls
     * - Audio parameters
     */
    template<typename T>
    [[nodiscard]]
    static inline T mapLog(
        T normalized,
        T minimum,
        T maximum) noexcept
    {
        static_assert(
            std::is_floating_point_v<T>);

        normalized =
            clamp(
                normalized,
                static_cast<T>(0),
                static_cast<T>(1));

        minimum =
            std::max(
                minimum,
                static_cast<T>(1e-12));

        const T ratio =
            maximum
            /
            minimum;

        return
            minimum
            *
            std::pow(
                ratio,
                normalized);
    }

    /**
     * @brief Wrap value into interval.
     *
     * Example:
     *
     * wrap(1.2,0,1) -> 0.2
     * wrap(-0.2,0,1) -> 0.8
     */
    template<typename T>
    [[nodiscard]]
    static inline T wrap(
        T value,
        T minimum,
        T maximum) noexcept
    {
        const T range =
            maximum
            -
            minimum;

        if (range <= static_cast<T>(0))
        {
            return minimum;
        }

        while (value >= maximum)
        {
            value -= range;
        }

        while (value < minimum)
        {
            value += range;
        }

        return value;
    }

    /**
     * @brief Return sign of value.
     *
     * Returns:
     *
     * -1
     *  0
     * +1
     */
    template<typename T>
    [[nodiscard]]
    static constexpr int sign(
        T value) noexcept
    {
        return
            (T(0) < value)
            -
            (value < T(0));
    }

    /**
     * @brief Detect denormal number.
     *
     * Denormals can severely impact CPU usage.
     */
    template<typename T>
    [[nodiscard]]
    static inline bool isDenormal(
        T value) noexcept
    {
        static_assert(
            std::is_floating_point_v<T>);

        return
            std::fpclassify(value)
            ==
            FP_SUBNORMAL;
    }

    /**
     * @brief Remove denormal values.
     *
     * Returns zero if value is denormal.
     */
    template<typename T>
    [[nodiscard]]
    static inline T killDenormal(
        T value) noexcept
    {
        static_assert(
            std::is_floating_point_v<T>);

        return
            isDenormal(value)
            ? static_cast<T>(0)
            : value;
    }

    /**
     * @brief Remove extremely small values.
     *
     * Faster alternative to fpclassify.
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T killTiny(
        T value,
        T threshold =
            static_cast<T>(1e-30)) noexcept
    {
        return
            (
                value > -threshold
                &&
                value < threshold
            )
            ? static_cast<T>(0)
            : value;
    }

    /**
     * @brief Normalize value to 0..1 range.
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T normalize(
        T value,
        T minimum,
        T maximum) noexcept
    {
        if (maximum == minimum)
        {
            return static_cast<T>(0);
        }

        return
            (
                value - minimum
            )
            /
            (
                maximum - minimum
            );
    }

    /**
     * @brief Check finite number.
     */
    template<typename T>
    [[nodiscard]]
    static inline bool isFinite(
        T value) noexcept
    {
        return std::isfinite(value);
    }

    /**
     * @brief Safe reciprocal.
     */
    template<typename T>
    [[nodiscard]]
    static inline T reciprocal(
        T value,
        T epsilon =
            static_cast<T>(1e-20)) noexcept
    {
        if (std::abs(value) < epsilon)
        {
            return static_cast<T>(0);
        }

        return
            static_cast<T>(1)
            /
            value;
    }

    /**
     * @brief Safe division.
     */
    template<typename T>
    [[nodiscard]]
    static inline T safeDivide(
        T numerator,
        T denominator,
        T epsilon =
            static_cast<T>(1e-20)) noexcept
    {
        if (std::abs(denominator) < epsilon)
        {
            return static_cast<T>(0);
        }

        return
            numerator
            /
            denominator;
    }

    /**
     * @brief Convert milliseconds to samples.
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T msToSamples(
        T milliseconds,
        T sampleRate) noexcept
    {
        return
            milliseconds
            *
            sampleRate
            *
            static_cast<T>(0.001);
    }

    /**
     * @brief Convert samples to milliseconds.
     */
    template<typename T>
    [[nodiscard]]
    static constexpr T samplesToMs(
        T samples,
        T sampleRate) noexcept
    {
        if (sampleRate <= static_cast<T>(0))
        {
            return static_cast<T>(0);
        }

        return
            samples
            *
            static_cast<T>(1000)
            /
            sampleRate;
    }
};

} // namespace cvdsp

#endif
