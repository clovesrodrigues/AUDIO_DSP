#ifndef CVDSP_CORE_TYPES_HPP
#define CVDSP_CORE_TYPES_HPP

/**
 * @file Types.hpp
 * @brief Fundamental fixed-width type aliases for the CV_DSP library.
 *
 * This header defines the minimal set of scalar aliases used by CV_DSP.
 * The aliases intentionally map directly to standard fixed-width integer
 * types and built-in floating-point types so the library remains portable
 * across VST3 SDK, iPlug2, JUCE, CLAP, and standalone host integrations.
 *
 * @note This header is real-time safe: it performs no allocation, contains no
 *       executable runtime work, and introduces no exceptions or RTTI usage.
 */

#include <cstdint>

namespace cvdsp
{
/**
 * @brief 32-bit IEEE-754 floating-point scalar alias.
 *
 * Used for single-precision DSP paths and host-facing sample buffers where the
 * framework exposes `float` audio samples.
 */
using f32 = float;

/**
 * @brief 64-bit IEEE-754 floating-point scalar alias.
 *
 * Used for double-precision DSP paths and host-facing sample buffers where the
 * framework exposes `double` audio samples.
 */
using f64 = double;

/**
 * @brief Signed 32-bit integer alias.
 *
 * Suitable for sample counts, channel indices, and bounded integer parameters
 * where a fixed 32-bit representation is required.
 */
using i32 = std::int32_t;

/**
 * @brief Unsigned 32-bit integer alias.
 *
 * Suitable for bit fields, packed identifiers, and non-negative counters where
 * a fixed 32-bit representation is required.
 */
using u32 = std::uint32_t;

/**
 * @brief Signed 64-bit integer alias.
 *
 * Suitable for large sample positions, timeline values, and fixed-width signed
 * counters used outside the audio callback.
 */
using i64 = std::int64_t;

/**
 * @brief Unsigned 64-bit integer alias.
 *
 * Suitable for large monotonic counters, packed identifiers, and fixed-width
 * unsigned values used by host integration layers.
 */
using u64 = std::uint64_t;
} // namespace cvdsp

#endif // CVDSP_CORE_TYPES_HPP
