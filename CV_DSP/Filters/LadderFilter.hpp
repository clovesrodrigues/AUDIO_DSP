#ifndef CVDSP_FILTERS_LADDERFILTER_HPP
#define CVDSP_FILTERS_LADDERFILTER_HPP

/**
 * @file LadderFilter.hpp
 * @brief Moog Inspired Ladder Filter
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - 4 Pole LowPass
 * - Resonance Feedback
 * - Drive Control
 * - Soft Saturation
 * - Real-Time Modulation
 */

#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>

namespace cvdsp
{

/**
 * @brief Moog-inspired Ladder Filter.
 *
 * 24 dB/oct LowPass
 *
 * Four cascaded one-pole stages.
 *
 * Resonance feedback path.
 *
 * Optional nonlinear drive.
 */
template<typename T>
class LadderFilter
{
    static_assert(
        std::is_floating_point_v<T>,
        "LadderFilter requires floating point type");

public:

    constexpr LadderFilter() noexcept = default;

    /**
     * @brief Initialize filter.
     */
    void prepare(
        T sampleRate) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        updateCoefficients();

        reset();
    }

    /**
     * @brief Reset internal states.
     */
    void reset() noexcept
    {
        z1_ =
            static_cast<T>(0);

        z2_ =
            static_cast<T>(0);

        z3_ =
            static_cast<T>(0);

        z4_ =
            static_cast<T>(0);
    }

    /**
     * @brief Set cutoff frequency.
     *
     * Safe for real-time modulation.
     */
    void setCutoff(
        T cutoffHz) noexcept
    {
        const T maxCutoff =
            sampleRate_
            *
            static_cast<T>(0.495);

        cutoffHz_ =
            std::clamp(
                cutoffHz,
                static_cast<T>(5),
                maxCutoff);

        updateCoefficients();
    }

    /**
     * @brief Set resonance amount.
     *
     * Range:
     *
     * 0.0 .. 4.0
     */
    void setResonance(
        T resonance) noexcept
    {
        resonance_ =
            std::clamp(
                resonance,
                static_cast<T>(0),
                static_cast<T>(4));

        updateFeedback();
    }

    /**
     * @brief Set input drive.
     *
     * Range:
     *
     * 1.0 .. 20.0
     */
    void setDrive(
        T drive) noexcept
    {
        drive_ =
            std::clamp(
                drive,
                static_cast<T>(1),
                static_cast<T>(20));
    }

    /**
     * @brief Process one sample.
     */
    inline T process(
        T input) noexcept
    {
        T x =
            input
            -
            (
                feedbackGain_
                *
                z4_
            );

        x *= drive_;

        x = saturate(x);

        z1_ += g_ * (x - z1_);
        z1_  = saturate(z1_);

        z2_ += g_ * (z1_ - z2_);
        z2_  = saturate(z2_);

        z3_ += g_ * (z2_ - z3_);
        z3_  = saturate(z3_);

        z4_ += g_ * (z3_ - z4_);
        z4_  = saturate(z4_);

        return z4_;
    }

private:

    /**
     * @brief Soft saturation.
     *
     * tanh approximation.
     */
    static inline T saturate(
        T x) noexcept
    {
        return std::tanh(x);
    }

    /**
     * @brief Update cutoff coefficient.
     */
    void updateCoefficients() noexcept
    {
        const T normalized =
            cutoffHz_
            /
            sampleRate_;

        const T omega =
            static_cast<T>(2)
            *
            std::numbers::pi_v<T>
            *
            normalized;

        g_ =
            static_cast<T>(1)
            -
            std::exp(
                -omega);

        g_ =
            std::clamp(
                g_,
                static_cast<T>(0),
                static_cast<T>(1));

        updateFeedback();
    }

    /**
     * @brief Resonance feedback scaling.
     */
    void updateFeedback() noexcept
    {
        feedbackGain_ =
            resonance_;
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T cutoffHz_ =
        static_cast<T>(1000);

    T resonance_ =
        static_cast<T>(0);

    T drive_ =
        static_cast<T>(1);

    T g_ =
        static_cast<T>(0);

    T feedbackGain_ =
        static_cast<T>(0);

    /**
     * Four ladder stages.
     */

    T z1_ =
        static_cast<T>(0);

    T z2_ =
        static_cast<T>(0);

    T z3_ =
        static_cast<T>(0);

    T z4_ =
        static_cast<T>(0);
};

} // namespace cvdsp

#endif
