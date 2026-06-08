#ifndef CVDSP_FILTERS_STATEVARIABLEFILTER_HPP
#define CVDSP_FILTERS_STATEVARIABLEFILTER_HPP

/**
 * @file StateVariableFilter.hpp
 * @brief Topology Preserving Transform State Variable Filter
 *
 * Features:
 * - LowPass
 * - HighPass
 * - BandPass
 * - Notch
 *
 * TPT (Topology Preserving Transform)
 * Real-Time Safe
 * Header-Only
 * C++20
 */

#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>

namespace cvdsp::filters
{

/**
 * @brief Filter response type.
 */
enum class SVFMode
{
    LowPass,
    HighPass,
    BandPass,
    Notch
};

/**
 * @brief Topology Preserving Transform State Variable Filter.
 *
 * Based on:
 *
 * Vadim Zavalishin
 * The Art of VA Filter Design
 *
 * Advantages:
 *
 * - Stable at high resonance
 * - Stable near Nyquist
 * - Real-time modulation friendly
 * - Continuous parameter updates
 * - Low numerical sensitivity
 */
template<typename T>
class StateVariableFilter
{
    static_assert(
        std::is_floating_point_v<T>,
        "StateVariableFilter requires floating point type");

public:

    constexpr StateVariableFilter() noexcept = default;

    /**
     * @brief Initialize filter.
     */
    void prepare(
        T sampleRate,
        SVFMode mode = SVFMode::LowPass) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        mode_ = mode;

        updateCoefficients();

        reset();
    }

    /**
     * @brief Reset internal states.
     */
    void reset() noexcept
    {
        ic1eq_ =
            static_cast<T>(0);

        ic2eq_ =
            static_cast<T>(0);
    }

    /**
     * @brief Set filter mode.
     */
    void setMode(
        SVFMode mode) noexcept
    {
        mode_ = mode;
    }

    /**
     * @brief Set cutoff frequency.
     *
     * Safe for sample-by-sample modulation.
     */
    void setCutoff(
        T cutoffHz) noexcept
    {
        constexpr T kMinCutoff =
            static_cast<T>(5);

        const T maxCutoff =
            sampleRate_
            *
            static_cast<T>(0.495);

        cutoffHz_ =
            std::clamp(
                cutoffHz,
                kMinCutoff,
                maxCutoff);

        updateCoefficients();
    }

    /**
     * @brief Set resonance.
     *
     * Q range:
     *
     * 0.1 .. 40.0
     */
    void setResonance(
        T q) noexcept
    {
        resonance_ =
            std::clamp(
                q,
                static_cast<T>(0.1),
                static_cast<T>(40.0));

        updateCoefficients();
    }

    /**
     * @brief Process one sample.
     */
    inline T process(
        T input) noexcept
    {
        const T v3 =
            input
            -
            ic2eq_;

        const T v1 =
            a1_
            *
            ic1eq_
            +
            a2_
            *
            v3;

        const T v2 =
            ic2eq_
            +
            a2_
            *
            ic1eq_
            +
            a3_
            *
            v3;

        ic1eq_ =
            static_cast<T>(2)
            *
            v1
            -
            ic1eq_;

        ic2eq_ =
            static_cast<T>(2)
            *
            v2
            -
            ic2eq_;

        const T lowpass =
            v2;

        const T bandpass =
            v1;

        const T highpass =
            input
            -
            k_
            *
            v1
            -
            v2;

        const T notch =
            highpass
            +
            lowpass;

        switch (mode_)
        {
            case SVFMode::LowPass:
                return lowpass;

            case SVFMode::HighPass:
                return highpass;

            case SVFMode::BandPass:
                return bandpass;

            case SVFMode::Notch:
                return notch;

            default:
                return lowpass;
        }
    }

private:

    /**
     * @brief Recalculate TPT coefficients.
     */
    void updateCoefficients() noexcept
    {
        const T wd =
            static_cast<T>(2)
            *
            std::numbers::pi_v<T>
            *
            cutoffHz_;

        const T Tinv =
            static_cast<T>(1)
            /
            sampleRate_;

        const T wa =
            static_cast<T>(2)
            *
            sampleRate_
            *
            std::tan(
                wd
                *
                Tinv
                *
                static_cast<T>(0.5));

        g_ =
            wa
            /
            (
                static_cast<T>(2)
                *
                sampleRate_
            );

        k_ =
            static_cast<T>(1)
            /
            resonance_;

        a1_ =
            static_cast<T>(1)
            /
            (
                static_cast<T>(1)
                +
                g_
                *
                (
                    g_
                    +
                    k_
                )
            );

        a2_ =
            g_
            *
            a1_;

        a3_ =
            g_
            *
            a2_;
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T cutoffHz_ =
        static_cast<T>(1000);

    T resonance_ =
        static_cast<T>(0.707);

    T g_ =
        static_cast<T>(0);

    T k_ =
        static_cast<T>(0);

    T a1_ =
        static_cast<T>(0);

    T a2_ =
        static_cast<T>(0);

    T a3_ =
        static_cast<T>(0);

    T ic1eq_ =
        static_cast<T>(0);

    T ic2eq_ =
        static_cast<T>(0);

    SVFMode mode_ =
        SVFMode::LowPass;
};

} // namespace cvdsp::filters

namespace cvdsp
{
using SVFMode = filters::SVFMode;

template<typename T>
using StateVariableFilter = filters::StateVariableFilter<T>;
} // namespace cvdsp

#endif
