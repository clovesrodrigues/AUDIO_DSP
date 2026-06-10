#ifndef CVDSP_GUITAR_PEDALS_PEDALMIX_HPP
#define CVDSP_GUITAR_PEDALS_PEDALMIX_HPP

/**
 * @file PedalMix.hpp
 * @brief Smoothed dry/wet mixer for guitar pedals.
 */

#include <type_traits>

#include "PedalParameterUtils.hpp"
#include "../../Core/ParameterSmoother.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T = float>
class PedalMix
{
    static_assert(std::is_floating_point_v<T>, "PedalMix requires a floating point type");

public:
    constexpr PedalMix() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        mixSmoother_.prepare(sampleRate_, PedalConstants<T>::kDefaultRampTimeSeconds);
        mixSmoother_.reset(mix_);
    }

    void reset() noexcept
    {
        mixSmoother_.reset(mix_);
    }

    void setMix(T normalizedMix) noexcept
    {
        mix_ = clamp01(normalizedMix);
        mixSmoother_.setTarget(mix_);
    }

    void setPhaseInvert(bool enabled) noexcept
    {
        phaseInvert_ = enabled;
    }

    [[nodiscard]] inline T processSample(T dry, T wet) noexcept
    {
        const T mix = mixSmoother_.process();
        const T wetSignal = phaseInvert_ ? -wet : wet;
        return dry * (static_cast<T>(1) - mix) + wetSignal * mix;
    }

    [[nodiscard]] T getMix() const noexcept
    {
        return mix_;
    }

    [[nodiscard]] bool isPhaseInverted() const noexcept
    {
        return phaseInvert_;
    }

private:
    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T mix_ { static_cast<T>(1) };
    bool phaseInvert_ { false };
    LinearSmoother<T> mixSmoother_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALMIX_HPP
