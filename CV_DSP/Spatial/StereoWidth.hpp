#ifndef CVDSP_SPATIAL_STEREOWIDTH_HPP
#define CVDSP_SPATIAL_STEREOWIDTH_HPP

/**
 * @file StereoWidth.hpp
 * @brief Stereo Width Processor
 *
 * Dependency:
 * - MidSide.hpp
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 */

#include <algorithm>
#include <type_traits>

#include "MidSide.hpp"

namespace cvdsp
{

/**
 * @brief Stereo Width Processor.
 *
 * Mid/Side based width control.
 *
 * Width:
 *
 * 0.0 = Mono
 * 1.0 = Original Width
 * 2.0 = 200% Width
 *
 * Processing:
 *
 * L/R
 *   ↓
 * M/S Encode
 *   ↓
 * Side Gain
 *   ↓
 * M/S Decode
 *   ↓
 * L/R
 */
template<typename T>
class StereoWidth
{
    static_assert(
        std::is_floating_point_v<T>,
        "StereoWidth requires floating point type");

public:

    /**
     * @brief Stereo frame.
     */
    struct StereoFrame
    {
        T left;
        T right;
    };

public:

    constexpr StereoWidth() noexcept = default;

    /**
     * @brief Process stereo frame.
     *
     * Width Range:
     *
     * 0.0 -> 2.0
     *
     * 0.0 = Mono
     * 1.0 = Original
     * 2.0 = 200%
     */
    [[nodiscard]]
    inline StereoFrame process(
        T left,
        T right,
        T width) const noexcept
    {
        width =
            std::clamp(
                width,
                static_cast<T>(0),
                static_cast<T>(2));

        const auto ms =
            MidSide<T>::encode(
                left,
                right);

        const T mid =
            ms.mid;

        const T side =
            ms.side
            *
            width;

        const auto lr =
            MidSide<T>::decode(
                mid,
                side);

        return
        {
            lr.left,
            lr.right
        };
    }

    /**
     * @brief Process in-place.
     */
    inline void process(
        T& left,
        T& right,
        T width) const noexcept
    {
        const auto result =
            process(
                left,
                right,
                width);

        left  = result.left;
        right = result.right;
    }
};

} // namespace cvdsp

#endif
