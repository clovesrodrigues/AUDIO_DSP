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
#include "../Core/DSPUtils.hpp"
#include "../Dynamics/EnvelopeFollower.hpp"
#include "../Filters/DCBlocker.hpp"
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
            DSPUtils::isFinite(sampleRate) && sampleRate > T(0)
            ? sampleRate
            : static_cast<T>(44100);

        triode_.prepare(
            sampleRate_);

        screenEnvelope_.prepare(
            sampleRate_);

        screenEnvelope_.setMode(
            dynamics::EnvelopeMode::Peak);

        screenEnvelope_.setAttackMs(
            static_cast<T>(4));

        screenEnvelope_.setReleaseMs(
            static_cast<T>(90));

        dcBlocker_.prepare(
            sampleRate_);

        dcBlocker_.setCutoffHz(
            static_cast<T>(18));

        updateTriodeSettings();

        reset();
    }

    void reset()
        noexcept
    {
        triode_.reset();
        screenEnvelope_.reset();
        dcBlocker_.reset();
    }

    void setDrive(
        T drive)
        noexcept
    {
        if (!DSPUtils::isFinite(drive))
        {
            drive = T(1);
        }

        drive_ =
            DSPUtils::clamp(
                drive,
                T(0),
                kMaxDrive);

        updateTriodeSettings();
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

        updateTriodeSettings();
    }

    void setScreenVoltage(
        T voltage)
        noexcept
    {
        if (!DSPUtils::isFinite(voltage))
        {
            voltage = kDefaultScreenVoltage;
        }

        screenVoltage_ =
            DSPUtils::clamp(
                voltage,
                kMinScreenVoltage,
                kMaxScreenVoltage);

        updateTriodeSettings();
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
        if (!DSPUtils::isFinite(input))
        {
            input = T(0);
        }

        input =
            DSPUtils::killTiny(
                input);

        /**
         * Pentode front-end.
         */

        T x =
            input
            *
            drive_;

        x += bias_;

        /**
         * Screen current memory.  This is intentionally based on the driven
         * control signal rather than on the already-clipped sample so that the
         * compression has attack/release behaviour instead of static per-sample
         * gain reduction.
         */

        const T screenEnvelope =
            screenEnvelope_.process(
                x);

        const T screenSupply =
            DSPUtils::clamp(
                T(1)
                -
                (
                    kScreenSagDepth
                    *
                    screenEnvelope
                ),
                kMinScreenSupply,
                T(1));

        /**
         * Screen voltage affects headroom. Lower dynamic screen supply reduces
         * headroom and increases power-tube compression.
         */

        const T screenFactor =
            std::max(
                (
                    screenVoltage_
                    /
                    kDefaultScreenVoltage
                )
                *
                screenSupply,
                kMinScreenFactor);

        /**
         * Pentode transfer.  The center is kept more linear than the companion
         * triode model, while the cubic term emphasizes odd harmonics typical
         * of pushed power tubes.
         */

        T shaped =
            std::tanh(
                x
                /
                screenFactor);

        const T oddAmount =
            static_cast<T>(0.16)
            +
            (
                (T(1) - screenSupply)
                *
                static_cast<T>(0.20)
            );

        const T odd =
            oddAmount
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
            static_cast<T>(0.82)
            *
            shaped
            +
            static_cast<T>(0.18)
            *
            Waveshaper<T>::process(
                shaped,
                WaveshaperMode::Arctan);

        /**
         * Natural power-tube compression from the smoothed screen-current
         * estimate.  This replaces the previous instantaneous sag term.
         */

        const T compression =
            T(1)
            /
            (
                T(1)
                +
                (
                    kCompressionDepth
                    *
                    screenEnvelope
                )
            );

        shaped *= compression;

        /**
         * Blend a smaller amount of triode behaviour for asymmetry and familiar
         * tube curvature.  Parameters are synchronized in the setters rather
         * than being rewritten every sample.
         */

        const T triodePart =
            triode_.process(
                input);

        shaped =
            static_cast<T>(0.78)
            *
            shaped
            +
            static_cast<T>(0.22)
            *
            triodePart;

        return
            DSPUtils::killTiny(
                dcBlocker_.process(
                    shaped
                    *
                    outputGain_));
    }

private:

    void updateTriodeSettings()
        noexcept
    {
        triode_.setDrive(
            drive_
            *
            static_cast<T>(0.35));

        triode_.setBias(
            bias_
            *
            static_cast<T>(0.45));

        triode_.setPlateVoltage(
            screenVoltage_
            *
            static_cast<T>(0.625));

        triode_.setOutputGain(
            static_cast<T>(1));
    }

private:

    static constexpr T kMaxDrive =
        static_cast<T>(32);

    static constexpr T kMaxBias =
        static_cast<T>(2);

    static constexpr T kMinScreenVoltage =
        static_cast<T>(100);

    static constexpr T kDefaultScreenVoltage =
        static_cast<T>(400);

    static constexpr T kMaxScreenVoltage =
        static_cast<T>(800);

    static constexpr T kMaxOutputGain =
        static_cast<T>(10);

    static constexpr T kScreenSagDepth =
        static_cast<T>(0.08);

    static constexpr T kCompressionDepth =
        static_cast<T>(0.10);

    static constexpr T kMinScreenSupply =
        static_cast<T>(0.55);

    static constexpr T kMinScreenFactor =
        static_cast<T>(0.25);

    TriodeStage<T>
        triode_;

    dynamics::EnvelopeFollower<T>
        screenEnvelope_;

    filters::DCBlocker<T>
        dcBlocker_;

    T sampleRate_ =
        static_cast<T>(44100);

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T screenVoltage_ =
        kDefaultScreenVoltage;

    T outputGain_ =
        static_cast<T>(1);
};

using PentodeStageF =
    PentodeStage<float>;

using PentodeStageD =
    PentodeStage<double>;

} // namespace cvdsp

#endif
