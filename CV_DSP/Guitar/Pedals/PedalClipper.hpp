#ifndef CVDSP_GUITAR_PEDALS_PEDALCLIPPER_HPP
#define CVDSP_GUITAR_PEDALS_PEDALCLIPPER_HPP

/**
 * @file PedalClipper.hpp
 * @brief Parametrized guitar-pedal waveshaper with drive, bias and asymmetry.
 */

#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>

#include "PedalParameterUtils.hpp"
#include "../../Core/ParameterSmoother.hpp"
#include "../../Math/FastMath.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T = float>
class PedalClipper
{
    static_assert(std::is_floating_point_v<T>, "PedalClipper requires a floating point type");

public:
    constexpr PedalClipper() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        driveSmoother_.prepare(sampleRate_, PedalConstants<T>::kDefaultRampTimeSeconds);
        biasSmoother_.prepare(sampleRate_, PedalConstants<T>::kDefaultRampTimeSeconds);
        driveSmoother_.reset(driveGain_);
        biasSmoother_.reset(bias_);
    }

    void reset() noexcept
    {
        driveSmoother_.reset(driveGain_);
        biasSmoother_.reset(bias_);
    }

    void setClipMode(PedalClipMode mode) noexcept { clipMode_ = mode; }
    void setQualityMode(PedalQualityMode mode) noexcept { qualityMode_ = mode; }

    void setDriveDb(T driveDb) noexcept
    {
        driveDb_ = std::clamp(driveDb, PedalConstants<T>::kMinDriveDb, PedalConstants<T>::kMaxDriveDb);
        driveGain_ = decibelsToGain(driveDb_);
        driveSmoother_.setTarget(driveGain_);
    }

    void setPositiveThreshold(T threshold) noexcept
    {
        positiveThreshold_ = clampThreshold(threshold);
        if (thresholdLink_)
            negativeThreshold_ = positiveThreshold_;
    }

    void setNegativeThreshold(T threshold) noexcept
    {
        negativeThreshold_ = clampThreshold(threshold);
        if (thresholdLink_)
            positiveThreshold_ = negativeThreshold_;
    }

    void setThresholdLink(bool enabled) noexcept
    {
        thresholdLink_ = enabled;
        if (thresholdLink_)
            negativeThreshold_ = positiveThreshold_;
    }

    void setKnee(T normalized) noexcept { knee_ = clamp01(normalized); }

    void setBias(T bipolarBias) noexcept
    {
        bias_ = std::clamp(bipolarBias, static_cast<T>(-1), static_cast<T>(1));
        biasSmoother_.setTarget(bias_);
    }

    void setAsymmetry(T normalized) noexcept { asymmetry_ = clamp01(normalized); }
    void setClipBlend(T normalized) noexcept { clipBlend_ = clamp01(normalized); }
    void setFoldbackAmount(T normalized) noexcept { foldbackAmount_ = clamp01(normalized); }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        const T driven = input * driveSmoother_.process() + biasSmoother_.process();
        const T threshold = driven >= static_cast<T>(0) ? effectivePositiveThreshold() : effectiveNegativeThreshold();
        const T normalized = driven / threshold;
        const T shaped = shape(normalized);
        return shaped * threshold;
    }

    [[nodiscard]] T getDriveDb() const noexcept { return driveDb_; }
    [[nodiscard]] PedalClipMode getClipMode() const noexcept { return clipMode_; }
    [[nodiscard]] T getPositiveThreshold() const noexcept { return positiveThreshold_; }
    [[nodiscard]] T getNegativeThreshold() const noexcept { return negativeThreshold_; }

private:
    [[nodiscard]] static constexpr T clampThreshold(T threshold) noexcept
    {
        return threshold < PedalConstants<T>::kMinClipThreshold
            ? PedalConstants<T>::kMinClipThreshold
            : (threshold > PedalConstants<T>::kMaxClipThreshold ? PedalConstants<T>::kMaxClipThreshold : threshold);
    }

    [[nodiscard]] T effectivePositiveThreshold() const noexcept
    {
        const T scale = static_cast<T>(1) - asymmetry_ * static_cast<T>(0.75);
        return clampThreshold(positiveThreshold_ * scale);
    }

    [[nodiscard]] T effectiveNegativeThreshold() const noexcept
    {
        const T scale = static_cast<T>(1) + asymmetry_ * static_cast<T>(0.75);
        return clampThreshold(negativeThreshold_ * scale);
    }

    [[nodiscard]] inline T shape(T x) const noexcept
    {
        const T primary = shapeMode(x, clipMode_);
        const T secondary = shapeMode(x, PedalClipMode::Tanh);
        return primary * (static_cast<T>(1) - clipBlend_) + secondary * clipBlend_;
    }

    [[nodiscard]] inline T shapeMode(T x, PedalClipMode mode) const noexcept
    {
        switch (mode)
        {
            case PedalClipMode::Hard: return hardClip(x);
            case PedalClipMode::Soft: return softClip(x);
            case PedalClipMode::Tanh: return tanhClip(x);
            case PedalClipMode::Arctan: return arctanClip(x);
            case PedalClipMode::Cubic: return cubicClip(x);
            case PedalClipMode::Foldback: return foldbackClip(x);
            case PedalClipMode::Hybrid: return hybridClip(x);
            default: return x;
        }
    }

    [[nodiscard]] static constexpr T hardClip(T x) noexcept
    {
        return x < static_cast<T>(-1) ? static_cast<T>(-1) : (x > static_cast<T>(1) ? static_cast<T>(1) : x);
    }

    [[nodiscard]] T softClip(T x) const noexcept
    {
        const T soft = fastSoftClip(x) * static_cast<T>(1.5);
        const T hard = hardClip(x);
        return soft * knee_ + hard * (static_cast<T>(1) - knee_);
    }

    [[nodiscard]] T tanhClip(T x) const noexcept
    {
        if (qualityMode_ == PedalQualityMode::Studio)
            return std::tanh(x);
        return fastTanh(x);
    }

    [[nodiscard]] T arctanClip(T x) const noexcept
    {
        constexpr T kScale = static_cast<T>(2) / std::numbers::pi_v<T>;
        if (qualityMode_ == PedalQualityMode::Studio)
            return std::atan(x) * kScale;
        return fastAtan(x) * kScale;
    }

    [[nodiscard]] static constexpr T cubicClip(T x) noexcept
    {
        const T clipped = hardClip(x);
        return static_cast<T>(1.5) * (clipped - clipped * clipped * clipped * static_cast<T>(1.0 / 3.0));
    }

    [[nodiscard]] T foldbackClip(T x) const noexcept
    {
        if (foldbackAmount_ <= static_cast<T>(0))
            return hardClip(x);

        T y = x;
        if (y > static_cast<T>(1) || y < static_cast<T>(-1))
        {
            y = std::fabs(std::fmod(y - static_cast<T>(1), static_cast<T>(4)));
            y = std::fabs(y - static_cast<T>(2)) - static_cast<T>(1);
        }

        return hardClip(x) * (static_cast<T>(1) - foldbackAmount_) + y * foldbackAmount_;
    }

    [[nodiscard]] T hybridClip(T x) const noexcept
    {
        const T soft = softClip(x);
        const T hard = hardClip(x * (static_cast<T>(1) + knee_));
        return soft * static_cast<T>(0.65) + hard * static_cast<T>(0.35);
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T driveDb_ { static_cast<T>(0) };
    T driveGain_ { static_cast<T>(1) };
    T positiveThreshold_ { PedalConstants<T>::kDefaultClipThreshold };
    T negativeThreshold_ { PedalConstants<T>::kDefaultClipThreshold };
    T knee_ { static_cast<T>(1) };
    T bias_ { static_cast<T>(0) };
    T asymmetry_ { static_cast<T>(0) };
    T clipBlend_ { static_cast<T>(0) };
    T foldbackAmount_ { static_cast<T>(0) };
    bool thresholdLink_ { true };
    PedalClipMode clipMode_ { PedalClipMode::Soft };
    PedalQualityMode qualityMode_ { PedalQualityMode::Normal };
    LinearSmoother<T> driveSmoother_ {};
    LinearSmoother<T> biasSmoother_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALCLIPPER_HPP
