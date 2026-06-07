#ifndef CVDSP_CORE_CONFIG_HPP
#define CVDSP_CORE_CONFIG_HPP

/**
 * @file Config.hpp
 * @brief Global compile-time configuration flags for CV_DSP.
 *
 * The values in this header describe default library policy and are expressed
 * as `inline constexpr` variables to avoid preprocessor-heavy configuration.
 * Projects may include this header from plug-in framework code, offline tools,
 * or tests without triggering allocation, exceptions, RTTI, or runtime setup.
 *
 * @note No DSP processing is implemented in this header.
 */

namespace cvdsp
{
/**
 * @brief Enables internal assertion checks in CV_DSP components.
 *
 * The foundational library defaults to enabled assertions so future debug-time
 * validation can be compiled from a single policy value without requiring
 * framework-specific macros. Assertion mechanisms must remain outside real-time
 * audio paths unless they are proven real-time safe.
 */
inline constexpr bool EnableAssertions = true;

/**
 * @brief Enables denormal-number protection policy for future DSP components.
 *
 * Denormal protection is enabled by default because subnormal floating-point
 * values can cause severe CPU spikes in real-time audio callbacks. Concrete DSP
 * modules should implement this policy without heap allocation and without
 * throwing exceptions inside `process()`.
 */
inline constexpr bool EnableDenormalProtection = true;
} // namespace cvdsp

#endif // CVDSP_CORE_CONFIG_HPP
