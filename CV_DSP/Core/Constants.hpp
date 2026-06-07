#ifndef CVDSP_CORE_CONSTANTS_HPP
#define CVDSP_CORE_CONSTANTS_HPP

/**
 * @file Constants.hpp
 * @brief Compile-time mathematical constants for CV_DSP.
 *
 * This header provides typed mathematical constants for single-precision and
 * double-precision DSP code. Constants are exposed as variable templates so
 * call sites can request the required scalar type without implicit narrowing.
 *
 * @note Constants are `inline constexpr` and therefore require no storage with
 *       external linkage beyond normal C++20 inline variable semantics.
 * @note This header is real-time safe: it performs no allocation, contains no
 *       executable runtime work, and introduces no exceptions or RTTI usage.
 */

#include "Types.hpp"

namespace cvdsp
{
/**
 * @brief Archimedes' constant π.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T pi = static_cast<T>(3.141592653589793238462643383279502884L);

/**
 * @brief Full-cycle radian constant 2π.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T twoPi = static_cast<T>(6.283185307179586476925286766559005768L);

/**
 * @brief Half-cycle quadrant radian constant π/2.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T halfPi = static_cast<T>(1.570796326794896619231321691639751442L);

/**
 * @brief Multiplicative inverse of π.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T invPi = static_cast<T>(0.318309886183790671537767526745028724L);

/**
 * @brief Principal square root of 2.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T sqrt2 = static_cast<T>(1.414213562373095048801688724209698079L);

/**
 * @brief Multiplicative inverse of the principal square root of 2.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T invSqrt2 = static_cast<T>(0.707106781186547524400844362104849039L);

/**
 * @brief Euler's number e, the base of the natural logarithm.
 * @tparam T Floating-point scalar type, typically cvdsp::f32 or cvdsp::f64.
 */
template <typename T>
inline constexpr T euler = static_cast<T>(2.718281828459045235360287471352662498L);
} // namespace cvdsp

#endif // CVDSP_CORE_CONSTANTS_HPP
