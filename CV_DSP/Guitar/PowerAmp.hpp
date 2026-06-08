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
        L6L6,
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
            sampleRate;

        stageA_.prepare(
            sampleRate);

        stageB_.prepare(
            sampleRate);

        configureModel();

        reset();
    }

    void reset()
        noexcept
    {
        stageA_.reset();
        stageB_.reset();

        sagState_ =
            T(0);
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
        /**
         * Dynamic supply sag.
         */

        const T envelope =
            std::abs(
                input);

        sagState_ +=
            sagCoefficient_
            *
            (
                envelope
                -
                sagState_
            );

        const T sagGain =
            T(1)
            -
            (
                sagAmount_
                *
                sagState_
            );

        T x =
            input
            *
            sagGain;

        /**
         * Push-Pull approximation.
         */

        x =
            stageA_.process(
                x);

        x =
            stageB_.process(
                x);

        /**
         * Dynamic compression.
         */

        const T compression =
            T(1)
            /
            (
                T(1)
                +
                compressionAmount_
                *
                std::abs(x)
            );

        x *= compression;

        return x;
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
                    static_cast<T>(2.8));

                stageA_.setBias(
                    static_cast<T>(0.05));

                stageB_.setBias(
                    static_cast<T>(-0.05));

                stageA_.setScreenVoltage(
                    static_cast<T>(420));

                stageB_.setScreenVoltage(
                    static_cast<T>(420));

                sagAmount_ =
                    static_cast<T>(0.12);

                compressionAmount_ =
                    static_cast<T>(0.30);

                break;
            }

            case Model::L6L6:
            {
                stageA_.setDrive(
                    static_cast<T>(2.2));

                stageB_.setDrive(
                    static_cast<T>(2.2));

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
                    static_cast<T>(3.8));

                stageA_.setScreenVoltage(
                    static_cast<T>(310));

                stageB_.setScreenVoltage(
                    static_cast<T>(310));

                sagAmount_ =
                    static_cast<T>(0.18);

                compressionAmount_ =
                    static_cast<T>(0.40);

                break;
            }
        }

        stageA_.setOutputGain(
            static_cast<T>(1));

        stageB_.setOutputGain(
            static_cast<T>(1));
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    Model model_ =
        Model::EL34;

    PentodeStage<T>
        stageA_;

    PentodeStage<T>
        stageB_;

    T sagState_ =
        T(0);

    T sagCoefficient_ =
        static_cast<T>(0.001);

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
