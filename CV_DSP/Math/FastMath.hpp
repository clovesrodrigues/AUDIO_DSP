#ifndef CVDSP_MATH_FASTMATH_HPP
#define CVDSP_MATH_FASTMATH_HPP

/**
 * @file FastMath.hpp
 * @brief Fast constexpr-friendly mathematical approximations for real-time DSP.
 *
 * This module intentionally favors deterministic, allocation-free arithmetic
 * over libm precision. The functions are small, force-inlined, `noexcept`, and
 * designed for hot audio paths where a bounded approximation is often more
 * useful than a fully rounded standard-library result.
 *
 * Theoretical scalar cost compared with common `std::` alternatives:
 *
 * | Function group | CV_DSP theoretical cost | Typical `std::` equivalent |
 * | --- | --- | --- |
 * | abs/min/max/clamp/sign | 1-3 comparisons, no calls | Similar for simple overloads; may not be force-inlined at every call site |
 * | lerp | 2 additions + 2 multiplications | `std::lerp` may add edge-case handling for exact IEEE behavior |
 * | wrap | 1 division + 1 floor-like cast + arithmetic | `std::fmod`/`std::remainder` usually call libm and handle many IEEE edge cases |
 * | pow2/pow3 | 1 or 2 multiplications | `std::pow(x, 2/3)` usually performs a generic exponent path |
 * | dB to gain | range reduction + cubic exp2 polynomial | `std::pow(10, db / 20)` usually calls libm |
 * | gain to dB | normalization + fifth-order atanh log polynomial | `std::log10` usually calls libm |
 * | tanh/atan/soft clip | low-order rational/polynomial approximation | `std::tanh`/`std::atan` usually call libm |
 *
 * @note These approximations are intended for audio-rate modulation, waveshaping,
 *       metering, and control smoothing. Use the standard library when exact IEEE
 *       special-value behavior or maximum numerical accuracy is required.
 * @note No external libraries are used. The header performs no allocation and
 *       does not throw exceptions.
 */

#include "../Core/Constants.hpp"
#include "../Core/Namespace.hpp"
#include "../Core/Types.hpp"

#include <type_traits>

namespace cvdsp
{
namespace detail
{
template <typename T>
CVDSP_FORCE_INLINE constexpr void requireArithmetic() noexcept
{
    static_assert(std::is_arithmetic_v<T>, "CV_DSP fast math functions require arithmetic scalar types.");
}

template <typename T>
CVDSP_FORCE_INLINE constexpr T fastFloor(T value) noexcept
{
    requireArithmetic<T>();

    if constexpr (std::is_integral_v<T>)
    {
        return value;
    }
    else
    {
        const auto truncated = static_cast<i64>(value);
        const auto asValue = static_cast<T>(truncated);
        return value < asValue ? static_cast<T>(truncated - 1) : asValue;
    }
}

template <typename T>
CVDSP_FORCE_INLINE constexpr T exp2Polynomial01(T fraction) noexcept
{
    // 2^f on [0, 1): cubic minimax-style Taylor approximation around zero.
    // Cost: 3 multiplications + 3 fused-add friendly additions.
    constexpr T ln2 = static_cast<T>(0.693147180559945309417232121458176568L);
    constexpr T c2 = static_cast<T>(0.240226506959100712333551263163332485L); // ln(2)^2 / 2
    constexpr T c3 = static_cast<T>(0.055504108664821579953005696965521856L); // ln(2)^3 / 6
    return static_cast<T>(1) + fraction * (ln2 + fraction * (c2 + fraction * c3));
}

template <typename T>
CVDSP_FORCE_INLINE constexpr T scaleByPowerOfTwo(T value, i64 exponent) noexcept
{
    // Cost: bounded repeated squaring over the exponent bits; no libm call.
    // The loop count is proportional to the number of bits in |exponent|, not
    // to the magnitude itself, and is therefore small for DSP control ranges.
    if (value == static_cast<T>(0))
    {
        return static_cast<T>(0);
    }

    T factor = static_cast<T>(2);
    T result = static_cast<T>(1);
    auto power = exponent < 0 ? static_cast<u64>(-exponent) : static_cast<u64>(exponent);

    while (power != 0U)
    {
        if ((power & 1U) != 0U)
        {
            result *= factor;
        }
        factor *= factor;
        power >>= 1U;
    }

    return exponent < 0 ? value / result : value * result;
}

template <typename T>
CVDSP_FORCE_INLINE constexpr T fastExp2(T value) noexcept
{
    requireArithmetic<T>();

    const auto integerPart = static_cast<i64>(fastFloor(value));
    const auto fraction = value - static_cast<T>(integerPart);
    return scaleByPowerOfTwo(exp2Polynomial01(fraction), integerPart);
}

template <typename T>
CVDSP_FORCE_INLINE constexpr T fastLog2Positive(T value) noexcept
{
    requireArithmetic<T>();

    if (value <= static_cast<T>(0))
    {
        return static_cast<T>(-120); // Practical floor for invalid/silent gains.
    }

    i64 exponent = 0;
    while (value >= static_cast<T>(2))
    {
        value *= static_cast<T>(0.5);
        ++exponent;
    }
    while (value < static_cast<T>(1))
    {
        value *= static_cast<T>(2);
        --exponent;
    }

    // log(m) = 2 * atanh((m - 1) / (m + 1)), m in [1, 2).
    // Cost: 1 division + 5 multiplications + additions; avoids std::log/log10.
    constexpr T invLn2 = static_cast<T>(1.442695040888963407359924681001892137L);
    const T y = (value - static_cast<T>(1)) / (value + static_cast<T>(1));
    const T y2 = y * y;
    const T ln = static_cast<T>(2) * y * (static_cast<T>(1) + y2 * (static_cast<T>(1.0 / 3.0) + y2 * static_cast<T>(1.0 / 5.0)));
    return static_cast<T>(exponent) + ln * invLn2;
}
} // namespace detail

/**
 * @brief Fast absolute value.
 * @details Cost: 1 comparison + optional negation; avoids function-call overhead
 *          and is comparable to `std::abs` for simple scalar types.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastAbs(T value) noexcept
{
    detail::requireArithmetic<T>();

    if constexpr (std::is_unsigned_v<T>)
    {
        return value;
    }
    else
    {
        return value < static_cast<T>(0) ? static_cast<T>(-value) : value;
    }
}

/**
 * @brief Fast minimum of two scalar values.
 * @details Cost: 1 comparison; comparable to `std::min` but force-inlined.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastMin(T a, T b) noexcept
{
    detail::requireArithmetic<T>();
    return b < a ? b : a;
}

/**
 * @brief Fast maximum of two scalar values.
 * @details Cost: 1 comparison; comparable to `std::max` but force-inlined.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastMax(T a, T b) noexcept
{
    detail::requireArithmetic<T>();
    return a < b ? b : a;
}

/**
 * @brief Clamp a value to the inclusive range [low, high].
 * @details Cost: 2 comparisons; comparable to `std::clamp` but force-inlined.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastClamp(T value, T low, T high) noexcept
{
    detail::requireArithmetic<T>();
    return fastMin(fastMax(value, low), high);
}

/**
 * @brief Linear interpolation between a and b.
 * @details Cost: 2 multiplications + 2 additions; simpler than `std::lerp`
 *          because it does not add special handling for all IEEE edge cases.
 */
template <typename T, typename U>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr auto fastLerp(T a, T b, U t) noexcept -> decltype(a + (b - a) * t)
{
    detail::requireArithmetic<T>();
    detail::requireArithmetic<U>();
    return a + (b - a) * t;
}

/**
 * @brief Wrap a value into the half-open range [low, high).
 * @details Floating-point cost: 1 division + floor-like cast + arithmetic;
 *          usually cheaper than `std::fmod`/`std::remainder` in scalar DSP
 *          paths. Integral cost: modulo + a correction branch.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastWrap(T value, T low, T high) noexcept
{
    detail::requireArithmetic<T>();

    const T range = high - low;
    if (range == static_cast<T>(0))
    {
        return low;
    }

    if constexpr (std::is_integral_v<T>)
    {
        T wrapped = static_cast<T>((value - low) % range);
        if (wrapped < static_cast<T>(0))
        {
            wrapped = static_cast<T>(wrapped + range);
        }
        return static_cast<T>(wrapped + low);
    }
    else
    {
        return value - range * detail::fastFloor((value - low) / range);
    }
}

/**
 * @brief Return -1 for negative values, 0 for zero, and +1 for positive values.
 * @details Cost: 2 comparisons; avoids branching-heavy sign helper code.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastSign(T value) noexcept
{
    detail::requireArithmetic<T>();
    return static_cast<T>((static_cast<T>(0) < value) - (value < static_cast<T>(0)));
}

/**
 * @brief Fast square (x^2).
 * @details Cost: 1 multiplication; much cheaper than generic `std::pow(x, 2)`.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastPow2(T value) noexcept
{
    detail::requireArithmetic<T>();
    return value * value;
}

/**
 * @brief Fast cube (x^3).
 * @details Cost: 2 multiplications; much cheaper than generic `std::pow(x, 3)`.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastPow3(T value) noexcept
{
    detail::requireArithmetic<T>();
    return value * value * value;
}

/**
 * @brief Approximate decibels to linear gain.
 * @details Cost: 1 multiplication + fast base-2 exponential approximation;
 *          avoids `std::pow(10, dB / 20)` and is suitable for smoothed gain
 *          changes and meters.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastDbToGain(T decibels) noexcept
{
    detail::requireArithmetic<T>();
    constexpr T dbToLog2Gain = static_cast<T>(0.166096404744368117393515971474267747L); // 1 / (20 * log10(2))
    return detail::fastExp2(decibels * dbToLog2Gain);
}

/**
 * @brief Approximate linear gain to decibels.
 * @details Cost: fast base-2 logarithm approximation + 1 multiplication;
 *          avoids `std::log10` and clamps non-positive gains to a practical
 *          silence floor before conversion.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastGainToDb(T gain) noexcept
{
    detail::requireArithmetic<T>();
    constexpr T log2ToDb = static_cast<T>(6.02059991327962390427477789448984365L); // 20 * log10(2)
    return detail::fastLog2Positive(gain) * log2ToDb;
}

/**
 * @brief Fast hyperbolic tangent approximation.
 * @details Cost: 1 division + 4 multiplications + additions. The rational
 *          approximation `x * (27 + x^2) / (27 + 9x^2)` is far cheaper than
 *          `std::tanh` and saturates explicitly outside a useful audio range.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastTanh(T value) noexcept
{
    detail::requireArithmetic<T>();

    if (value <= static_cast<T>(-3))
    {
        return static_cast<T>(-1);
    }
    if (value >= static_cast<T>(3))
    {
        return static_cast<T>(1);
    }

    const T x2 = value * value;
    return value * (static_cast<T>(27) + x2) / (static_cast<T>(27) + static_cast<T>(9) * x2);
}

/**
 * @brief Fast arctangent approximation in radians.
 * @details Cost for |x| <= 1: 1 division + 2 multiplications + additions;
 *          large magnitudes use the reciprocal identity and remain much cheaper
 *          than `std::atan` for real-time waveshaping/filter code.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastAtan(T value) noexcept
{
    detail::requireArithmetic<T>();

    const T absValue = fastAbs(value);
    const T sign = fastSign(value);

    if (absValue <= static_cast<T>(1))
    {
        return value / (static_cast<T>(1) + static_cast<T>(0.28) * value * value);
    }

    const T reciprocal = static_cast<T>(1) / absValue;
    const T small = reciprocal / (static_cast<T>(1) + static_cast<T>(0.28) * reciprocal * reciprocal);
    return sign * (halfPi<T> - small);
}

/**
 * @brief Cubic soft-clip waveshaper.
 * @details Cost in the active region: 2 multiplications + subtraction. This is
 *          far cheaper than tanh-based clipping with `std::tanh` and has a
 *          continuous first derivative at the saturation boundary.
 */
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T fastSoftClip(T value) noexcept
{
    detail::requireArithmetic<T>();

    if (value <= static_cast<T>(-1))
    {
        return static_cast<T>(-2.0 / 3.0);
    }
    if (value >= static_cast<T>(1))
    {
        return static_cast<T>(2.0 / 3.0);
    }

    return value - fastPow3(value) * static_cast<T>(1.0 / 3.0);
}
} // namespace cvdsp

#endif // CVDSP_MATH_FASTMATH_HPP
