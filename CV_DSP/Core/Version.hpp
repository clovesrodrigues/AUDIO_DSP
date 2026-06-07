#ifndef CVDSP_CORE_VERSION_HPP
#define CVDSP_CORE_VERSION_HPP

/**
 * @file Version.hpp
 * @brief Semantic version metadata for CV_DSP.
 *
 * This header centralizes version information for the header-only CV_DSP
 * foundation. Version values are compile-time constants so plug-in wrappers,
 * diagnostics, documentation generators, and tests can query the library
 * version without runtime initialization.
 *
 * @note This header is real-time safe: it performs no allocation, contains no
 *       executable runtime work, and introduces no exceptions or RTTI usage.
 */

namespace cvdsp
{
/** @brief Major semantic version component. */
inline constexpr int Major = 0;

/** @brief Minor semantic version component. */
inline constexpr int Minor = 1;

/** @brief Patch semantic version component. */
inline constexpr int Patch = 0;

/**
 * @brief Null-terminated semantic version string.
 *
 * The string is intentionally represented as a string literal with static
 * storage duration. It requires no allocation and can be referenced from host
 * metadata code or diagnostic output.
 */
inline constexpr const char* VersionString = "0.1.0";
} // namespace cvdsp

#endif // CVDSP_CORE_VERSION_HPP
