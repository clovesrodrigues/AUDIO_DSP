#ifndef CVDSP_GUITAR_PEDALS_PEDALGAINSTAGE_HPP
#define CVDSP_GUITAR_PEDALS_PEDALGAINSTAGE_HPP

/**
 * @file PedalGainStage.hpp
 * @brief Smoothed input/output gain helper for guitar pedals.
 */

#include <algorithm>
#include <type_traits>

#include "PedalParameterUtils.hpp"
#include "../../Core/ParameterSmoother.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T = float>
class PedalGainStage
{
    static_assert(std::is_floating_point_v<T>, "PedalGainStage requires a floating point type");

public:
    constexpr PedalGainStage() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        gainSmoother_.prepare(sampleRate_, PedalConstants<T>::kDefaultRampTimeSeconds);
        gainSmoother_.reset(gainLinear_);
    }

    void reset() noexcept
    {
        gainSmoother_.reset(gainLinear_);
    }

    void setGainDb(T gainDb) noexcept
    {
        gainDb_ = std::clamp(gainDb, PedalConstants<T>::kMinGainDb, PedalConstants<T>::kMaxGainDb);
        gainLinear_ = decibelsToGain(gainDb_);
        gainSmoother_.setTarget(gainLinear_);
    }

    void setGainLinear(T gain) noexcept
    {
        gainLinear_ = std::clamp(
            gain,
            decibelsToGain(PedalConstants<T>::kMinGainDb),
            decibelsToGain(PedalConstants<T>::kMaxGainDb));
        gainDb_ = std::clamp(
            DSPUtils::gainToDb(gainLinear_),
            PedalConstants<T>::kMinGainDb,
            PedalConstants<T>::kMaxGainDb);
        gainSmoother_.setTarget(gainLinear_);
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        return input * gainSmoother_.process();
    }

    [[nodiscard]] T getGainDb() const noexcept
    {
        return gainDb_;
    }

    [[nodiscard]] T getGainLinear() const noexcept
    {
        return gainLinear_;
    }

private:
    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T gainDb_ { static_cast<T>(0) };
    T gainLinear_ { static_cast<T>(1) };
    LinearSmoother<T> gainSmoother_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALGAINSTAGE_HPP
