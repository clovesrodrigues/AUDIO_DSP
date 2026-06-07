#ifndef CVDSP_SATURATION_WAVESHAPER_HPP
#define CVDSP_SATURATION_WAVESHAPER_HPP

/**
 * @file Waveshaper.hpp
 * @brief Collection of Waveshaping Algorithms
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 */

#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>

namespace cvdsp
{

/**
 * @brief Waveshaper modes.
 */
enum class WaveshaperMode
{
    HardClip,
    SoftClip,
    Tanh,
    Arctan,
    Cubic,
    Foldback
};

/**
 * @brief Collection of static waveshaping algorithms.
 *
 * Stateless.
 *
 * Thread-safe.
 *
 * Real-Time Safe.
 */
template<typename T>
class Waveshaper
{
    static_assert(
        std::is_floating_point_v<T>,
        "Waveshaper requires floating point type");

public:

    /**
     * @brief Process sample.
     *
     * @param input Input sample.
     * @param mode Waveshaper mode.
     *
     * @return Processed sample.
     */
    static inline T process(
        T input,
        WaveshaperMode mode) noexcept
    {
        switch (mode)
        {
            case WaveshaperMode::HardClip:
                return hardClip(input);

            case WaveshaperMode::SoftClip:
                return softClip(input);

            case WaveshaperMode::Tanh:
                return tanhShape(input);

            case WaveshaperMode::Arctan:
                return arctanShape(input);

            case WaveshaperMode::Cubic:
                return cubicShape(input);

            case WaveshaperMode::Foldback:
                return foldbackShape(input);

            default:
                return input;
        }
    }

private:

    /**
     * @brief Hard Clipper.
     *
     * Abrupt clipping.
     */
    static inline T hardClip(
        T x) noexcept
    {
        return std::clamp(
            x,
            static_cast<T>(-1),
            static_cast<T>(1));
    }

    /**
     * @brief Soft Clipper.
     *
     * Smooth transition.
     */
    static inline T softClip(
        T x) noexcept
    {
        constexpr T threshold =
            static_cast<T>(1);

        if (x > threshold)
        {
            return threshold;
        }

        if (x < -threshold)
        {
            return -threshold;
        }

        return
            x
            -
            (
                x
                *
                x
                *
                x
                /
                static_cast<T>(3)
            );
    }

    /**
     * @brief Hyperbolic tangent.
     */
    static inline T tanhShape(
        T x) noexcept
    {
        return std::tanh(x);
    }

    /**
     * @brief Arctangent saturator.
     */
    static inline T arctanShape(
        T x) noexcept
    {
        constexpr T kScale =
            static_cast<T>(2)
            /
            std::numbers::pi_v<T>;

        return
            std::atan(x)
            *
            kScale;
    }

    /**
     * @brief Cubic saturation.
     *
     * Third-order polynomial.
     */
    static inline T cubicShape(
        T x) noexcept
    {
        constexpr T limit =
            static_cast<T>(1);

        x =
            std::clamp(
                x,
                -limit,
                limit);

        return
            x
            -
            (
                static_cast<T>(1.0 / 3.0)
                *
                x
                *
                x
                *
                x
            );
    }

    /**
     * @brief Foldback distortion.
     */
    static inline T foldbackShape(
        T x) noexcept
    {
        constexpr T threshold =
            static_cast<T>(1);

        if (x > threshold || x < -threshold)
        {
            x =
                std::fabs(
                    std::fmod(
                        x - threshold,
                        threshold * static_cast<T>(4)));

            x =
                std::fabs(
                    x
                    -
                    threshold
                    *
                    static_cast<T>(2))
                -
                threshold;
        }

        return x;
    }
};

} // namespace cvdsp

#endif
