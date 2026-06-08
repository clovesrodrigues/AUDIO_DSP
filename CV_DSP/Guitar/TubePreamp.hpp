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
 * - Core/DSPUtils.hpp
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
#include "../Core/DSPUtils.hpp"
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
            DSPUtils::isFinite(sampleRate) && sampleRate > T(0)
            ? sampleRate
            : static_cast<T>(44100);

        for (auto& stage : stages_)
        {
            stage.prepare(
                sampleRate_);
        }

        driveSmoother_.prepare(
            sampleRate_,
            static_cast<T>(0.02));

        biasSmoother_.prepare(
            sampleRate_,
            static_cast<T>(0.02));

        plateVoltageSmoother_.prepare(
            sampleRate_,
            static_cast<T>(0.02));

        outputSmoother_.prepare(
            sampleRate_,
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

        biasSmoother_.reset(
            bias_);

        plateVoltageSmoother_.reset(
            plateVoltage_);

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
        if (!DSPUtils::isFinite(drive))
        {
            drive = T(0);
        }

        drive_ =
            DSPUtils::clamp(
                drive,
                T(0),
                kMaxDrive);

        driveSmoother_.setTarget(
            drive_);
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

        biasSmoother_.setTarget(
            bias_);
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

        plateVoltageSmoother_.setTarget(
            plateVoltage_);
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

        outputSmoother_.setTarget(
            outputGain_);
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

        const T bias =
            biasSmoother_.process();

        const T plateVoltage =
            plateVoltageSmoother_.process();

        const T output =
            outputSmoother_.process();

        if (!DSPUtils::isFinite(input))
        {
            input = T(0);
        }

        T x =
            DSPUtils::killTiny(
                input);

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
                bias);

            stages_[i].setPlateVoltage(
                plateVoltage);

            stages_[i].setOutputGain(
                computeStageOutputGain(
                    i));

            x =
                stages_[i].process(
                    x);
        }

        x *= output;

        if (!DSPUtils::isFinite(x))
        {
            return T(0);
        }

        return
            DSPUtils::killTiny(
                x);
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

    T computeStageOutputGain(
        std::size_t stage)
        const noexcept
    {
        if (stage + 1 >= numStages_)
        {
            return T(1);
        }

        switch (stage)
        {
            case 0:
                return static_cast<T>(0.85);

            case 1:
                return static_cast<T>(0.80);

            case 2:
                return static_cast<T>(0.75);

            default:
                return T(1);
        }
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

    std::size_t numStages_ = 1;

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T plateVoltage_ =
        kDefaultPlateVoltage;

    T outputGain_ =
        static_cast<T>(1);

    std::array<
        TriodeStage<T>,
        MaxStages>
        stages_;

    OnePoleSmoother<T>
        driveSmoother_;

    OnePoleSmoother<T>
        biasSmoother_;

    OnePoleSmoother<T>
        plateVoltageSmoother_;

    OnePoleSmoother<T>
        outputSmoother_;
};

using TubePreampF =
    TubePreamp<float>;

using TubePreampD =
    TubePreamp<double>;

} // namespace cvdsp

#endif
