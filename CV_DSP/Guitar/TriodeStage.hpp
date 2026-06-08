#ifndef CVDSP_GUITAR_TRIODESTAGE_HPP
#define CVDSP_GUITAR_TRIODESTAGE_HPP

/**
 * @file TriodeStage.hpp
 * @brief Triode Tube Gain Stage
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - Saturation/Waveshaper.hpp
 * - Core/DSPUtils.hpp
 * - Filters/DCBlocker.hpp
 * - Math/FastMath.hpp
 * - Math/Oversampling.hpp
 *
 * Optional:
 *
 * - Core/ParameterSmoother.hpp
 *
 * Features:
 *
 * - Triode Inspired Transfer Function
 * - Even Harmonic Generation
 * - Asymmetrical Saturation
 * - Plate Voltage Control
 * - DC Blocking
 * - Oversampling Processing
 * - Guitar Preamp Style Nonlinearity
 *
 * No allocation in process().
 */

#include <cmath>
#include <type_traits>

#include "../Saturation/Waveshaper.hpp"
#include "../Core/DSPUtils.hpp"
#include "../Filters/DCBlocker.hpp"
#include "../Math/FastMath.hpp"
#include "../Math/Oversampling.hpp"

namespace cvdsp
{

template<typename T>
class TriodeStage
{
    static_assert(
        std::is_floating_point_v<T>,
        "TriodeStage requires floating point type");

public:

    TriodeStage() = default;

    void prepare(
        T sampleRate)
        noexcept
    {
        sampleRate_ =
            DSPUtils::isFinite(sampleRate) && sampleRate > T(0)
            ? sampleRate
            : static_cast<T>(44100);

        oversampler_.prepare(
            sampleRate_);

        dcBlocker_.prepare(
            sampleRate_);

        dcBlocker_.setCutoffHz(
            static_cast<T>(20));

        reset();
    }

    void reset()
        noexcept
    {
        oversampler_.reset();

        dcBlocker_.reset();
    }

    void setDrive(
        T drive)
        noexcept
    {
        if (!DSPUtils::isFinite(drive))
        {
            drive = T(0);
        }

        drive_ =
            DSPUtils::clamp(
                drive,
                T(0),
                kMaxDrive);
    }

    void setBias(
        T bias)
        noexcept
    {
        if (!DSPUtils::isFinite(bias))
        {
            bias = T(0);
        }

        bias_ =
            DSPUtils::clamp(
                bias,
                -kMaxBias,
                kMaxBias);
    }

    void setPlateVoltage(
        T voltage)
        noexcept
    {
        if (!DSPUtils::isFinite(voltage))
        {
            voltage = kDefaultPlateVoltage;
        }

        plateVoltage_ =
            DSPUtils::clamp(
                voltage,
                kMinPlateVoltage,
                kMaxPlateVoltage);
    }

    void setOutputGain(
        T gain)
        noexcept
    {
        if (!DSPUtils::isFinite(gain))
        {
            gain = T(1);
        }

        outputGain_ =
            DSPUtils::clamp(
                gain,
                T(0),
                kMaxOutputGain);
    }

    [[nodiscard]]
    T getDrive() const noexcept
    {
        return drive_;
    }

    [[nodiscard]]
    T getBias() const noexcept
    {
        return bias_;
    }

    [[nodiscard]]
    T getPlateVoltage() const noexcept
    {
        return plateVoltage_;
    }

    [[nodiscard]]
    T getOutputGain() const noexcept
    {
        return outputGain_;
    }

    T process(
        T input)
        noexcept
    {
        if (!DSPUtils::isFinite(input))
        {
            input = T(0);
        }

        input =
            DSPUtils::killTiny(
                input);

        const auto upsampled =
            oversampler_.processUp(
                input);

        typename Oversampling<T, 4>::OversampledBlock
            processed{};

        constexpr std::size_t factor =
            4;

        for (std::size_t i = 0;
             i < factor;
             ++i)
        {
            processed[i] =
                processTriode(
                    upsampled[i]);
        }

        return
            DSPUtils::killTiny(
                dcBlocker_.process(
                    oversampler_
                        .processDown(
                            processed)));
    }

private:

    T processTriode(
        T x)
        noexcept
    {
        /**
         * Input gain stage.
         */

        x *= drive_;

        /**
         * Bias point.
         */

        x += bias_;

        /**
         * Plate voltage scaling.
         *
         * Lower voltage:
         * more compression.
         *
         * Higher voltage:
         * more headroom.
         */

        const T plateFactor =
            plateVoltage_
            /
            static_cast<T>(250);

        /**
         * Koren-inspired curvature.
         */

        const T positive =
            static_cast<T>(1.15);

        const T negative =
            static_cast<T>(0.75);

        T shaped;

        if (x >= T(0))
        {
            shaped =
                positive
                *
                fastTanh(
                    x
                    /
                    plateFactor);
        }
        else
        {
            shaped =
                negative
                *
                fastTanh(
                    x
                    /
                    plateFactor);
        }

        /**
         * Even harmonic enhancement.
         */

        const T even =
            static_cast<T>(0.15)
            *
            shaped
            *
            shaped;

        shaped += even;

        /**
         * Additional tube saturation.
         */

        shaped =
            static_cast<T>(0.85)
            *
            shaped
            +
            static_cast<T>(0.15)
            *
            Waveshaper<T>::process(
                shaped,
                WaveshaperMode::Tanh);

        return
            DSPUtils::killTiny(
                shaped
                *
                outputGain_);
    }

private:

    static constexpr T kMaxDrive =
        static_cast<T>(64);

    static constexpr T kMaxBias =
        static_cast<T>(4);

    static constexpr T kMinPlateVoltage =
        static_cast<T>(50);

    static constexpr T kDefaultPlateVoltage =
        static_cast<T>(250);

    static constexpr T kMaxPlateVoltage =
        static_cast<T>(500);

    static constexpr T kMaxOutputGain =
        static_cast<T>(10);

    T sampleRate_ =
        static_cast<T>(44100);

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T plateVoltage_ =
        kDefaultPlateVoltage;

    T outputGain_ =
        static_cast<T>(1);

    Oversampling<
        T,
        4>
        oversampler_;

    filters::DCBlocker<T> dcBlocker_;
};

using TriodeStageF =
    TriodeStage<float>;

using TriodeStageD =
    TriodeStage<double>;

} // namespace cvdsp

#endif
