#ifndef CVDSP_GUITAR_POWERAMP_HPP
#define CVDSP_GUITAR_POWERAMP_HPP

/**
 * @file PowerAmp.hpp
 * @brief Guitar Power Amplifier Simulation
 *
 * Models:
 *
 * - EL34
 * - 6L6
 * - KT88
 * - EL84
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - PentodeStage.hpp
 *
 * Optional:
 *
 * - Presence Filter
 * - Resonance Filter
 *
 * No allocations.
 * No exceptions.
 */

#include <cmath>
#include <type_traits>

#include "PentodeStage.hpp"
#include "../Core/DSPUtils.hpp"
#include "../Dynamics/EnvelopeFollower.hpp"

namespace cvdsp
{

template<typename T>
class PowerAmp
{
public:

    static_assert(
        std::is_floating_point_v<T>,
        "PowerAmp requires floating point type");

    enum class Model
    {
        EL34,
        SixL6,
        L6L6 = SixL6,
        KT88,
        EL84
    };

public:

    PowerAmp() = default;

    void prepare(
        T sampleRate)
        noexcept
    {
        sampleRate_ =
            DSPUtils::isFinite(sampleRate) && sampleRate > T(0)
            ? sampleRate
            : static_cast<T>(44100);

        stageA_.prepare(
            sampleRate_);

        stageB_.prepare(
            sampleRate_);

        sagEnvelope_.prepare(
            sampleRate_);

        sagEnvelope_.setMode(
            dynamics::EnvelopeMode::Peak);

        sagEnvelope_.setAttackMs(
            static_cast<T>(8));

        sagEnvelope_.setReleaseMs(
            static_cast<T>(140));

        compressionEnvelope_.prepare(
            sampleRate_);

        compressionEnvelope_.setMode(
            dynamics::EnvelopeMode::Peak);

        compressionEnvelope_.setAttackMs(
            static_cast<T>(3));

        compressionEnvelope_.setReleaseMs(
            static_cast<T>(70));

        configureModel();

        reset();
    }

    void reset()
        noexcept
    {
        stageA_.reset();
        stageB_.reset();

        sagEnvelope_.reset();
        compressionEnvelope_.reset();
    }

    void setModel(
        Model model)
        noexcept
    {
        model_ =
            model;

        configureModel();
    }

    Model getModel() const noexcept
    {
        return model_;
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
         * Dynamic supply sag.  The detector has attack/release memory and is
         * sample-rate invariant through EnvelopeFollower coefficients.
         */

        const T sagEnvelope =
            sagEnvelope_.process(
                input);

        const T sagGain =
            DSPUtils::clamp(
                T(1)
                -
                (
                    sagAmount_
                    *
                    sagEnvelope
                ),
                kMinSagGain,
                T(1));

        const T driven =
            input
            *
            sagGain;

        /**
         * Push-pull approximation: split phase, process complementary pentode
         * branches, then recombine.  For a linear pair this collapses to unity;
         * under bias/saturation it yields partial even-harmonic cancellation and
         * stronger power-stage odd harmonics.
         */

        const T positive =
            stageA_.process(
                driven);

        const T negative =
            -stageB_.process(
                -driven);

        T x =
            static_cast<T>(0.5)
            *
            (
                positive
                +
                negative
            );

        /**
         * Dynamic compression after the virtual output pair.  This uses a second
         * envelope so fast transients are retained while sustained level bends
         * the supply/transfer response naturally.
         */

        const T compressionEnvelope =
            compressionEnvelope_.process(
                x);

        const T compression =
            T(1)
            /
            (
                T(1)
                +
                compressionAmount_
                *
                compressionEnvelope
            );

        x *= compression;

        return
            DSPUtils::killTiny(
                x);
    }

private:

    void configureModel()
        noexcept
    {
        switch (model_)
        {
            case Model::EL34:
            {
                stageA_.setDrive(
                    static_cast<T>(3.0));

                stageB_.setDrive(
                    static_cast<T>(3.0));

                stageA_.setBias(
                    static_cast<T>(0.045));

                stageB_.setBias(
                    static_cast<T>(-0.045));

                stageA_.setScreenVoltage(
                    static_cast<T>(420));

                stageB_.setScreenVoltage(
                    static_cast<T>(420));

                sagAmount_ =
                    static_cast<T>(0.12);

                compressionAmount_ =
                    static_cast<T>(0.28);

                break;
            }

            case Model::SixL6:
            {
                stageA_.setDrive(
                    static_cast<T>(2.2));

                stageB_.setDrive(
                    static_cast<T>(2.2));

                stageA_.setBias(
                    static_cast<T>(0.025));

                stageB_.setBias(
                    static_cast<T>(-0.025));

                stageA_.setScreenVoltage(
                    static_cast<T>(460));

                stageB_.setScreenVoltage(
                    static_cast<T>(460));

                sagAmount_ =
                    static_cast<T>(0.08);

                compressionAmount_ =
                    static_cast<T>(0.18);

                break;
            }

            case Model::KT88:
            {
                stageA_.setDrive(
                    static_cast<T>(1.8));

                stageB_.setDrive(
                    static_cast<T>(1.8));

                stageA_.setBias(
                    static_cast<T>(0.015));

                stageB_.setBias(
                    static_cast<T>(-0.015));

                stageA_.setScreenVoltage(
                    static_cast<T>(520));

                stageB_.setScreenVoltage(
                    static_cast<T>(520));

                sagAmount_ =
                    static_cast<T>(0.04);

                compressionAmount_ =
                    static_cast<T>(0.10);

                break;
            }

            case Model::EL84:
            {
                stageA_.setDrive(
                    static_cast<T>(4.0));

                stageB_.setDrive(
                    static_cast<T>(4.0));

                stageA_.setBias(
                    static_cast<T>(0.060));

                stageB_.setBias(
                    static_cast<T>(-0.060));

                stageA_.setScreenVoltage(
                    static_cast<T>(310));

                stageB_.setScreenVoltage(
                    static_cast<T>(310));

                sagAmount_ =
                    static_cast<T>(0.18);

                compressionAmount_ =
                    static_cast<T>(0.38);

                break;
            }
        }

        stageA_.setOutputGain(
            static_cast<T>(1));

        stageB_.setOutputGain(
            static_cast<T>(1));
    }

private:

    static constexpr T kMinSagGain =
        static_cast<T>(0.35);

    T sampleRate_ =
        static_cast<T>(44100);

    Model model_ =
        Model::EL34;

    PentodeStage<T>
        stageA_;

    PentodeStage<T>
        stageB_;

    dynamics::EnvelopeFollower<T>
        sagEnvelope_;

    dynamics::EnvelopeFollower<T>
        compressionEnvelope_;

    T sagAmount_ =
        static_cast<T>(0.10);

    T compressionAmount_ =
        static_cast<T>(0.20);
};

using PowerAmpF =
    PowerAmp<float>;

using PowerAmpD =
    PowerAmp<double>;

} // namespace cvdsp

#endif
