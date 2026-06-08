#ifndef CVDSP_MATH_OVERSAMPLING_HPP
#define CVDSP_MATH_OVERSAMPLING_HPP

/**
 * @file Oversampling.hpp
 * @brief Oversampling Engine
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - 2x Oversampling
 * - 4x Oversampling
 * - 8x Oversampling
 * - Upsampling
 * - Downsampling
 * - Anti-Imaging Filter
 * - Anti-Aliasing Filter
 *
 * No dynamic allocation inside process.
 */

#include <array>
#include <cstddef>
#include <type_traits>

namespace cvdsp
{

/**
 * @brief Supported oversampling factors.
 */
enum class OversamplingFactor
{
    x2 = 2,
    x4 = 4,
    x8 = 8
};

/**
 * @brief Simple Halfband FIR.
 *
 * Linear phase.
 *
 * Used for:
 * - Anti Imaging
 * - Anti Aliasing
 *
 * Symmetric coefficients.
 */
template<typename T>
class HalfbandFIR
{
    static_assert(
        std::is_floating_point_v<T>,
        "HalfbandFIR requires floating point type");

public:

    static constexpr std::size_t NumTaps = 7;

    constexpr HalfbandFIR() noexcept = default;

    void reset() noexcept
    {
        buffer_.fill(
            static_cast<T>(0));

        index_ = 0;
    }

    inline T process(
        T input) noexcept
    {
        buffer_[index_] = input;

        T output =
            coeffs_[0] * get(0)
            +
            coeffs_[1] * get(1)
            +
            coeffs_[2] * get(2)
            +
            coeffs_[3] * get(3)
            +
            coeffs_[4] * get(4)
            +
            coeffs_[5] * get(5)
            +
            coeffs_[6] * get(6);

        index_++;

        if (index_ >= NumTaps)
        {
            index_ = 0;
        }

        return output;
    }

private:

    inline T get(
        std::size_t delay) const noexcept
    {
        std::size_t pos =
            (
                index_
                +
                NumTaps
                -
                delay
            )
            %
            NumTaps;

        return buffer_[pos];
    }

private:

    std::array<T, NumTaps> buffer_{};

    std::size_t index_ = 0;

    static constexpr std::array<T, NumTaps> coeffs_
    {
        static_cast<T>(-0.045),
        static_cast<T>(0.0),
        static_cast<T>(0.275),
        static_cast<T>(0.540),
        static_cast<T>(0.275),
        static_cast<T>(0.0),
        static_cast<T>(-0.045)
    };
};

/**
 * @brief Oversampling engine.
 *
 * Template factor:
 *
 * 2
 * 4
 * 8
 */
template<
    typename T,
    std::size_t Factor>
class Oversampling
{
    static_assert(
        std::is_floating_point_v<T>,
        "Oversampling requires floating point type");

    static_assert(
        Factor == 2 ||
        Factor == 4 ||
        Factor == 8,
        "Factor must be 2,4 or 8");

public:

    static constexpr std::size_t OversamplingFactorValue =
        Factor;

    static constexpr std::size_t BlockSize =
        Factor;

    using OversampledBlock =
        std::array<T, Factor>;

public:

    constexpr Oversampling() noexcept = default;

    /**
     * @brief Initialize.
     */
    void prepare(
        T sampleRate) noexcept
    {
        sampleRate_ = sampleRate;

        reset();
    }

    /**
     * @brief Reset state.
     */
    void reset() noexcept
    {
        upFilter_.reset();
        downFilter_.reset();
    }

    /**
     * @brief Upsample one sample.
     *
     * Returns Factor samples.
     */
    [[nodiscard]]
    inline OversampledBlock processUp(
        T input) noexcept
    {
        OversampledBlock block{};

        /**
         * Zero-stuffing.
         */
        block[0] =
            upFilter_.process(
                input);

        for (std::size_t i = 1;
             i < Factor;
             ++i)
        {
            block[i] =
                upFilter_.process(
                    static_cast<T>(0));
        }

        return block;
    }

    /**
     * @brief Downsample oversampled block.
     *
     * Anti-aliasing filtering
     * before decimation.
     */
    [[nodiscard]]
    inline T processDown(
        const OversampledBlock& block) noexcept
    {
        T filtered =
            static_cast<T>(0);

        for (std::size_t i = 0;
             i < Factor;
             ++i)
        {
            filtered =
                downFilter_.process(
                    block[i]);
        }

        return filtered;
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    HalfbandFIR<T> upFilter_;
    HalfbandFIR<T> downFilter_;
};

using Oversampling2xF  = Oversampling<float, 2>;
using Oversampling4xF  = Oversampling<float, 4>;
using Oversampling8xF  = Oversampling<float, 8>;

using Oversampling2xD  = Oversampling<double, 2>;
using Oversampling4xD  = Oversampling<double, 4>;
using Oversampling8xD  = Oversampling<double, 8>;

} // namespace cvdsp

#endif
