#ifndef CVDSP_GUITAR_TUBEPREAMP_HPP
#define CVDSP_GUITAR_TUBEPREAMP_HPP

/**
 * @file TubePreamp.hpp
 * @brief Cascaded Triode Tube Preamp
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - Guitar/TriodeStage.hpp
 * - Core/ParameterSmoother.hpp
 *
 * Optional:
 *
 * - Math/Oversampling.hpp
 *
 * Features:
 *
 * - 1 Stage
 * - 2 Stages
 * - 3 Stages
 * - 4 Stages
 *
 * - Cascaded Gain Structure
 * - High Gain Preamp Topology
 * - Smooth Parameter Control
 *
 * No allocations inside process().
 */

#include <cstddef>
#include <array>
#include <type_traits>

#include "TriodeStage.hpp"
#include "../Core/ParameterSmoother.hpp"

namespace cvdsp
{

template<typename T>
class TubePreamp
{
    static_assert(
        std::is_floating_point_v<T>,
        "TubePreamp requires floating point type");

public:

    static constexpr std::size_t
        MaxStages = 4;

public:

    TubePreamp() = default;

    void prepare(
        T sampleRate)
        noexcept
    {
        sampleRate_ =
            sampleRate;

        for (auto& stage : stages_)
        {
            stage.prepare(
                sampleRate);
        }

        driveSmoother_.prepare(
            sampleRate,
            static_cast<T>(0.02));

        outputSmoother_.prepare(
            sampleRate,
            static_cast<T>(0.02));

        reset();
    }

    void reset()
        noexcept
    {
        for (auto& stage : stages_)
        {
            stage.reset();
        }

        driveSmoother_.reset(
            drive_);

        outputSmoother_.reset(
            outputGain_);
    }

    void setNumStages(
        std::size_t stages)
        noexcept
    {
        if (stages < 1)
        {
            stages = 1;
        }

        if (stages > MaxStages)
        {
            stages = MaxStages;
        }

        numStages_ = stages;
    }

    void setDrive(
        T drive)
        noexcept
    {
        drive_ = drive;

        driveSmoother_.setTargetValue(
            drive);
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
        plateVoltage_ =
            voltage;
    }

    void setOutputGain(
        T gain)
        noexcept
    {
        outputGain_ = gain;

        outputSmoother_.setTargetValue(
            gain);
    }

    [[nodiscard]]
    std::size_t getNumStages()
        const noexcept
    {
        return numStages_;
    }

    [[nodiscard]]
    T getDrive()
        const noexcept
    {
        return drive_;
    }

    [[nodiscard]]
    T getBias()
        const noexcept
    {
        return bias_;
    }

    [[nodiscard]]
    T getPlateVoltage()
        const noexcept
    {
        return plateVoltage_;
    }

    [[nodiscard]]
    T getOutputGain()
        const noexcept
    {
        return outputGain_;
    }

    T process(
        T input)
        noexcept
    {
        const T drive =
            driveSmoother_.process();

        const T output =
            outputSmoother_.process();

        T x = input;

        for (std::size_t i = 0;
             i < numStages_;
             ++i)
        {
            const T stageDrive =
                computeStageDrive(
                    drive,
                    i);

            stages_[i].setDrive(
                stageDrive);

            stages_[i].setBias(
                bias_);

            stages_[i].setPlateVoltage(
                plateVoltage_);

            stages_[i].setOutputGain(
                static_cast<T>(1));

            x =
                stages_[i].process(
                    x);
        }

        return x * output;
    }

private:

    T computeStageDrive(
        T globalDrive,
        std::size_t stage)
        const noexcept
    {
        switch (stage)
        {
            case 0:
                return
                    globalDrive;

            case 1:
                return
                    globalDrive
                    *
                    static_cast<T>(0.90);

            case 2:
                return
                    globalDrive
                    *
                    static_cast<T>(0.80);

            case 3:
                return
                    globalDrive
                    *
                    static_cast<T>(0.70);

            default:
                return
                    globalDrive;
        }
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    std::size_t numStages_ = 1;

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T plateVoltage_ =
        static_cast<T>(250);

    T outputGain_ =
        static_cast<T>(1);

    std::array<
        TriodeStage<T>,
        MaxStages>
        stages_;

    ParameterSmoother<T>
        driveSmoother_;

    ParameterSmoother<T>
        outputSmoother_;
};

using TubePreampF =
    TubePreamp<float>;

using TubePreampD =
    TubePreamp<double>;

} // namespace cvdsp

#endif
