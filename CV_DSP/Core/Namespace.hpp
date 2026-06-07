#ifndef CVDSP_CORE_NAMESPACE_HPP
#define CVDSP_CORE_NAMESPACE_HPP

/**
 * @file Namespace.hpp
 * @brief Minimal portability annotations for CV_DSP declarations.
 *
 * This header intentionally defines only the smallest macro surface needed by
 * the foundation layer. The macros are limited to compiler attributes that are
 * useful for real-time DSP code while keeping CV_DSP compatible with VST3 SDK,
 * iPlug2, JUCE, CLAP, and standalone C++20 builds.
 *
 * @note No namespace-opening or namespace-closing macros are provided; use the
 *       explicit `namespace cvdsp { ... }` form in source code for clarity.
 */

/**
 * @def CVDSP_FORCE_INLINE
 * @brief Requests aggressive inlining for small real-time safe functions.
 *
 * The macro maps to compiler-specific force-inline attributes when available
 * and falls back to standard `inline` elsewhere. It should be used sparingly on
 * short functions that are expected to sit on the audio processing hot path.
 */
#if defined(_MSC_VER)
#define CVDSP_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CVDSP_FORCE_INLINE inline __attribute__((always_inline))
#else
#define CVDSP_FORCE_INLINE inline
#endif

/**
 * @def CVDSP_NODISCARD
 * @brief Marks return values that should not be ignored by callers.
 *
 * The macro maps directly to the C++20 `[[nodiscard]]` attribute and exists as
 * a stable annotation point for future compiler-specific policy if required.
 */
#define CVDSP_NODISCARD [[nodiscard]]

namespace cvdsp
{
} // namespace cvdsp

#endif // CVDSP_CORE_NAMESPACE_HPP
