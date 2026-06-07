#ifndef CVDSP_FILTERS_ALLPASSFILTER_HPP
#define CVDSP_FILTERS_ALLPASSFILTER_HPP

/**
 * @file AllPassFilter.hpp
 * @brief All-Pass Filter
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - Phase Rotation
 * - Diffusion
 * - Schroeder Reverb Building Block
 * - Phaser Building Block
 */

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "../Delay/DelayLine.hpp"

namespace cvdsp
{

/**
 * @brief Feedback All-Pass Filter.
 *
 * Transfer Function:
 *
 *              -g + z^-N
 * H(z) = -----------------------
 *          1 - g * z^-N
 *
 * where:
 *
 * g = feedback coefficient
 * N = delay length
 *
 * Magnitude Response:
 *
 * |H(z)| = 1
 *
 * Only phase is modified.
 */
template<typename T>
class AllPassFilter
{
    static_assert(
        std::is_floating_point_v<T>,
        "AllPassFilter requires floating point type");

public:

    constexpr AllPassFilter() noexcept = default;

    /**
     * @brief Initialize filter.
     *
     * @param sampleRate Processing sample rate.
     * @param maxDelaySamples Maximum delay size.
     */
    void prepare(
        T sampleRate,
        std::size_t maxDelaySamples) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        maxDelaySamples_ =
            std::max<std::size_t>(
                maxDelaySamples,
                1u);

        delay_.prepareSamples(
            maxDelaySamples_);

        reset();
    }

    /**
     * @brief Reset internal state.
     */
    void reset() noexcept
    {
        delay_.reset();

        delaySamples_ = 1;
    }

    /**
     * @brief Set delay length.
     *
     * Range:
     *
     * 1 .. maxDelaySamples
     */
    void setDelaySamples(
        std::size_t delaySamples) noexcept
    {
        delaySamples_ =
            std::clamp(
                delaySamples,
                static_cast<std::size_t>(1),
                maxDelaySamples_);
    }

    /**
     * @brief Set feedback coefficient.
     *
     * Range:
     *
     * -0.999 .. +0.999
     */
    void setFeedback(
        T feedback) noexcept
    {
        feedback_ =
            std::clamp(
                feedback,
                static_cast<T>(-0.999),
                static_cast<T>(0.999));
    }

    /**
     * @brief Process one sample.
     *
     * Canonical Schroeder all-pass:
     *
     * y[n] =
     * -g*x[n]
     * +
     * d[n]
     *
     * write =
     * x[n]
     * +
     * g*y[n]
     */
    inline T process(
        T input) noexcept
    {
        const T delayed =
            delay_.readSamples(
                delaySamples_);

        const T output =
            (-feedback_ * input)
            +
            delayed;

        const T writeSample =
            input
            +
            (
                feedback_
                *
                output
            );

        delay_.write(
            writeSample);

        return output;
    }

private:

    DelayLine<T> delay_;

    T sampleRate_ =
        static_cast<T>(44100);

    T feedback_ =
        static_cast<T>(0.5);

    std::size_t delaySamples_ =
        1;

    std::size_t maxDelaySamples_ =
        1;
};

} // namespace cvdsp

#endif
