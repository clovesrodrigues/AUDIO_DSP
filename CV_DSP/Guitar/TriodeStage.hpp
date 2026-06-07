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
 * - Oversampling Processing
 * - Guitar Preamp Style Nonlinearity
 *
 * No allocation in process().
 */

#include <cmath>
#include <type_traits>

#include "../Saturation/Waveshaper.hpp"
#include "../Core/DSPUtils.hpp"
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
            sampleRate;

        oversampler_.prepare(
            sampleRate);

        reset();
    }

    void reset()
        noexcept
    {
        oversampler_.reset();
    }

    void setDrive(
        T drive)
        noexcept
    {
        drive_ =
            drive < T(0)
            ? T(0)
            : drive;
    }

    void setBias(
        T bias)
        noexcept
    {
        bias_ = bias;
    }

    void setPlateVoltage(
        T voltage)
        noexcept
    {
        if (voltage < T(50))
        {
            voltage = T(50);
        }

        if (voltage > T(500))
        {
            voltage = T(500);
        }

        plateVoltage_ =
            voltage;
    }

    void setOutputGain(
        T gain)
        noexcept
    {
        outputGain_ = gain;
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
        oversampler_.processUp(
            input);

        T accumulated =
            T(0);

        constexpr std::size_t factor =
            4;

        for (std::size_t i = 0;
             i < factor;
             ++i)
        {
            accumulated +=
                processTriode(
                    oversampler_
                        .getUpsampledSample(
                            i));
        }

        return
            oversampler_
                .processDown(
                    accumulated
                    /
                    static_cast<T>(
                        factor));
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
                std::tanh(
                    x
                    /
                    plateFactor);
        }
        else
        {
            shaped =
                negative
                *
                std::tanh(
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

        /**
         * Remove DC shift
         * introduced by asymmetry.
         */

        shaped -=
            static_cast<T>(0.15)
            *
            bias_;

        return
            shaped
            *
            outputGain_;
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T plateVoltage_ =
        static_cast<T>(250);

    T outputGain_ =
        static_cast<T>(1);

    Oversampling<
        T,
        4>
        oversampler_;
};

using TriodeStageF =
    TriodeStage<float>;

using TriodeStageD =
    TriodeStage<double>;

} // namespace cvdsp

#endif
