#ifndef CVDSP_GUITAR_PENTODESTAGE_HPP
#define CVDSP_GUITAR_PENTODESTAGE_HPP

/**
 * @file PentodeStage.hpp
 * @brief Pentode Power Tube Stage
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - TriodeStage.hpp
 * - Waveshaper.hpp
 *
 * Optional:
 *
 * - Oversampling.hpp
 *
 * Features:
 *
 * - Pentode Inspired Saturation
 * - Power Tube Compression
 * - Screen Voltage Control
 * - Even/Odd Harmonic Generation
 * - Push Toward Power Amp Behaviour
 *
 * No allocations in process().
 */

#include <cmath>
#include <type_traits>

#include "TriodeStage.hpp"
#include "../Saturation/Waveshaper.hpp"

namespace cvdsp
{

template<typename T>
class PentodeStage
{
    static_assert(
        std::is_floating_point_v<T>,
        "PentodeStage requires floating point type");

public:

    PentodeStage() = default;

    void prepare(
        T sampleRate)
        noexcept
    {
        sampleRate_ =
            sampleRate;

        triode_.prepare(
            sampleRate);

        reset();
    }

    void reset()
        noexcept
    {
        triode_.reset();
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

    void setScreenVoltage(
        T voltage)
        noexcept
    {
        if (voltage < T(100))
        {
            voltage = T(100);
        }

        if (voltage > T(800))
        {
            voltage = T(800);
        }

        screenVoltage_ =
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
    T getScreenVoltage() const noexcept
    {
        return screenVoltage_;
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
        /**
         * Pentode front-end.
         */

        T x =
            input
            *
            drive_;

        x += bias_;

        /**
         * Screen voltage affects
         * headroom and compression.
         */

        const T screenFactor =
            screenVoltage_
            /
            static_cast<T>(400);

        /**
         * Pentode transfer.
         *
         * More linear center region
         * than triodes.
         */

        T shaped =
            std::tanh(
                x
                /
                screenFactor);

        /**
         * Generate stronger odd harmonics.
         */

        const T odd =
            static_cast<T>(0.20)
            *
            shaped
            *
            shaped
            *
            shaped;

        shaped += odd;

        /**
         * Pentode clipping characteristic.
         */

        shaped =
            static_cast<T>(0.80)
            *
            shaped
            +
            static_cast<T>(0.20)
            *
            Waveshaper<T>::process(
                shaped,
                WaveshaperMode::Arctan);

        /**
         * Simulate screen sag.
         */

        const T sag =
            static_cast<T>(1)
            /
            (
                static_cast<T>(1)
                +
                static_cast<T>(0.15)
                *
                std::abs(
                    shaped));

        shaped *= sag;

        /**
         * Blend some triode behaviour.
         */

        triode_.setDrive(
            drive_
            *
            static_cast<T>(0.4));

        triode_.setBias(
            bias_
            *
            static_cast<T>(0.5));

        triode_.setOutputGain(
            static_cast<T>(1));

        const T triodePart =
            triode_.process(
                input);

        shaped =
            static_cast<T>(0.70)
            *
            shaped
            +
            static_cast<T>(0.30)
            *
            triodePart;

        return
            shaped
            *
            outputGain_;
    }

private:

    TriodeStage<T>
        triode_;

    T sampleRate_ =
        static_cast<T>(44100);

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T screenVoltage_ =
        static_cast<T>(400);

    T outputGain_ =
        static_cast<T>(1);
};

using PentodeStageF =
    PentodeStage<float>;

using PentodeStageD =
    PentodeStage<double>;

} // namespace cvdsp

#endif
