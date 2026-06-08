#ifndef CVDSP_SPATIAL_MIDSIDE_HPP
#define CVDSP_SPATIAL_MIDSIDE_HPP

/**
 * @file MidSide.hpp
 * @brief Mid/Side Encoder and Decoder
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - Mid/Side Encode
 * - Mid/Side Decode
 * - Stereo Manipulation
 * - Mastering Support
 */

#include <type_traits>

namespace cvdsp::spatial
{

/**
 * @brief Mid/Side stereo utilities.
 *
 * Static-only class.
 *
 * No internal state.
 */
template<typename T>
class MidSide
{
    static_assert(
        std::is_floating_point_v<T>,
        "MidSide requires floating point type");

public:

    /**
     * @brief Mid/Side frame.
     */
    struct MSFrame
    {
        T mid;
        T side;
    };

    /**
     * @brief Stereo frame.
     */
    struct StereoFrame
    {
        T left;
        T right;
    };

    /**
     * @brief Encode Left/Right to Mid/Side.
     *
     * Energy preserving form:
     *
     * Mid  = (L + R) / sqrt(2)
     * Side = (L - R) / sqrt(2)
     */
    [[nodiscard]]
    static constexpr MSFrame encode(
        T left,
        T right) noexcept
    {
        constexpr T kNorm =
            static_cast<T>(
                0.70710678118654752440);

        return
        {
            (left + right) * kNorm,
            (left - right) * kNorm
        };
    }

    /**
     * @brief Decode Mid/Side to Left/Right.
     *
     * L = (M + S) / sqrt(2)
     * R = (M - S) / sqrt(2)
     */
    [[nodiscard]]
    static constexpr StereoFrame decode(
        T mid,
        T side) noexcept
    {
        constexpr T kNorm =
            static_cast<T>(
                0.70710678118654752440);

        return
        {
            (mid + side) * kNorm,
            (mid - side) * kNorm
        };
    }

    /**
     * @brief Compute Mid only.
     */
    [[nodiscard]]
    static constexpr T mid(
        T left,
        T right) noexcept
    {
        constexpr T kNorm =
            static_cast<T>(
                0.70710678118654752440);

        return
            (left + right)
            * kNorm;
    }

    /**
     * @brief Compute Side only.
     */
    [[nodiscard]]
    static constexpr T side(
        T left,
        T right) noexcept
    {
        constexpr T kNorm =
            static_cast<T>(
                0.70710678118654752440);

        return
            (left - right)
            * kNorm;
    }
};

} // namespace cvdsp::spatial

namespace cvdsp
{
template<typename T>
using MidSide = spatial::MidSide<T>;
} // namespace cvdsp

#endif
