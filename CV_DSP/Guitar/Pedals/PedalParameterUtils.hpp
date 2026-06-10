#ifndef CVDSP_GUITAR_PEDALS_PEDALPARAMETERUTILS_HPP
#define CVDSP_GUITAR_PEDALS_PEDALPARAMETERUTILS_HPP

/**
 * @file PedalParameterUtils.hpp
 * @brief Allocation-free normalized parameter mapping helpers for guitar pedals.
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "PedalTypes.hpp"
#include "../../Core/DSPUtils.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T>
[[nodiscard]] constexpr T clamp01(T value) noexcept
{
    static_assert(std::is_floating_point_v<T>, "clamp01 requires a floating point type");
    return value < static_cast<T>(0) ? static_cast<T>(0) : (value > static_cast<T>(1) ? static_cast<T>(1) : value);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToLinear(T minimum, T maximum, T normalized) noexcept
{
    static_assert(std::is_floating_point_v<T>, "normalizedToLinear requires a floating point type");
    const T n = clamp01(normalized);
    return minimum + (maximum - minimum) * n;
}

template<typename T>
[[nodiscard]] inline T normalizedToLogFrequency(T minimumHz, T maximumHz, T normalized) noexcept
{
    static_assert(std::is_floating_point_v<T>, "normalizedToLogFrequency requires a floating point type");
    minimumHz = std::max(minimumHz, PedalConstants<T>::kMinFrequencyHz);
    maximumHz = std::max(maximumHz, minimumHz);
    return DSPUtils::mapLog(clamp01(normalized), minimumHz, maximumHz);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToDecibels(T minimumDb, T maximumDb, T normalized) noexcept
{
    return normalizedToLinear(minimumDb, maximumDb, normalized);
}

template<typename T>
[[nodiscard]] inline T decibelsToGain(T decibels) noexcept
{
    static_assert(std::is_floating_point_v<T>, "decibelsToGain requires a floating point type");
    return DSPUtils::dbToGain(decibels);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToQ(T minimumQ, T maximumQ, T normalized) noexcept
{
    static_assert(std::is_floating_point_v<T>, "normalizedToQ requires a floating point type");
    minimumQ = minimumQ < PedalConstants<T>::kMinQ ? PedalConstants<T>::kMinQ : minimumQ;
    maximumQ = maximumQ < minimumQ ? minimumQ : maximumQ;
    return normalizedToLinear(minimumQ, maximumQ, normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToBipolar(T normalized) noexcept
{
    return normalizedToLinear(static_cast<T>(-1), static_cast<T>(1), normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToThreshold(T minimumThreshold, T maximumThreshold, T normalized) noexcept
{
    static_assert(std::is_floating_point_v<T>, "normalizedToThreshold requires a floating point type");
    minimumThreshold = minimumThreshold < PedalConstants<T>::kMinClipThreshold ? PedalConstants<T>::kMinClipThreshold : minimumThreshold;
    maximumThreshold = maximumThreshold < minimumThreshold ? minimumThreshold : maximumThreshold;
    return normalizedToLinear(minimumThreshold, maximumThreshold, normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToMilliseconds(T minimumMs, T maximumMs, T normalized) noexcept
{
    static_assert(std::is_floating_point_v<T>, "normalizedToMilliseconds requires a floating point type");
    minimumMs = minimumMs < static_cast<T>(0) ? static_cast<T>(0) : minimumMs;
    maximumMs = maximumMs < minimumMs ? minimumMs : maximumMs;
    return normalizedToLinear(minimumMs, maximumMs, normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToPercent(T normalized) noexcept
{
    return clamp01(normalized) * static_cast<T>(100);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToDriveDb(T normalized) noexcept
{
    return normalizedToDecibels(PedalConstants<T>::kMinDriveDb, PedalConstants<T>::kMaxDriveDb, normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToInputGainDb(T normalized) noexcept
{
    return normalizedToDecibels(PedalConstants<T>::kMinInputGainDb, PedalConstants<T>::kMaxInputGainDb, normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToOutputGainDb(T normalized) noexcept
{
    return normalizedToDecibels(PedalConstants<T>::kMinOutputGainDb, PedalConstants<T>::kMaxOutputGainDb, normalized);
}

template<typename T>
[[nodiscard]] inline T normalizedToGuitarCutoffHz(T normalized) noexcept
{
    return normalizedToLogFrequency(PedalConstants<T>::kMinGuitarFrequencyHz, PedalConstants<T>::kMaxGuitarFrequencyHz, normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToMusicalQ(T normalized) noexcept
{
    return normalizedToQ(static_cast<T>(0.3), static_cast<T>(8), normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToDefaultThreshold(T normalized) noexcept
{
    return normalizedToThreshold(static_cast<T>(0.05), static_cast<T>(2), normalized);
}

template<typename T>
[[nodiscard]] constexpr T normalizedToBias(T normalized) noexcept
{
    return normalizedToBipolar(normalized);
}

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALPARAMETERUTILS_HPP
