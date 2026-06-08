#ifndef CVDSP_MANAGER_PARAMETERSTATE_HPP
#define CVDSP_MANAGER_PARAMETERSTATE_HPP

/**
 * @file ParameterState.hpp
 * @brief Runtime state for one CV_DSP parameter.
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * This header stores the mutable value state for a single parameter described by
 * ParameterDescriptor. It owns no descriptor metadata, performs no dynamic
 * allocation, uses no RTTI, throws no exceptions, and does not depend on any host
 * SDK. Automation queues, preset serialization, host ID mapping, and modulation
 * routing belong to higher-level manager or adapter layers.
 *
 * The state uses the concrete smoothers provided by ParameterSmoother.hpp:
 * LinearSmoother, ExponentialSmoother, and OnePoleSmoother. No generic
 * ParameterSmoother<T> type is introduced.
 */

#include "../Core/Namespace.hpp"
#include "../Core/ParameterSmoother.hpp"
#include "../Core/Types.hpp"
#include "ParameterDescriptor.hpp"

#include <type_traits>

namespace cvdsp::manager
{

/**
 * @brief Runtime smoothing strategy for ParameterState.
 *
 * Discrete, Boolean, and Enum parameters are applied immediately even when a
 * smoothing mode other than None is selected, because interpolating through
 * intermediate real values would create invalid states for those value kinds.
 */
enum class ParameterSmoothingMode : u32
{
    None = 0,     ///< Apply targets immediately with no smoothing.
    Linear,       ///< Use cvdsp::LinearSmoother<T>.
    Exponential,  ///< Use cvdsp::ExponentialSmoother<T>.
    OnePole       ///< Use cvdsp::OnePoleSmoother<T>.
};

/**
 * @brief Configuration used to prepare the internal smoothing strategies.
 */
template<typename T = f32>
struct ParameterSmoothingConfig
{
    static_assert(
        std::is_floating_point_v<T>,
        "ParameterSmoothingConfig requires a floating point type");

    T sampleRate = static_cast<T>(44100);          ///< Host sample rate in Hz.
    T rampTimeSeconds = static_cast<T>(0.005);     ///< Linear/exponential default ramp time.
    T exponentialCurve = static_cast<T>(5);        ///< Exponential ramp curvature.
    T onePoleTimeConstant = static_cast<T>(0.010); ///< One-pole time constant in seconds.
};

/**
 * @brief Mutable runtime state for a single parameter.
 *
 * ParameterState binds to one immutable ParameterDescriptor and stores current
 * and target values in both real and normalized domains. The real value is the
 * value consumed by DSP processors. The normalized target is the value expected
 * by host automation and preset systems. During smoothing, currentNormalized is
 * updated when the immediate/current real value is updated; managers that only
 * need host-facing state should prefer getTargetNormalized() to avoid relying on
 * intermediate smoothing positions.
 *
 * @tparam T Floating-point value type, normally cvdsp::f32 or cvdsp::f64.
 */
template<typename T = f32>
class ParameterState
{
    static_assert(
        std::is_floating_point_v<T>,
        "ParameterState requires a floating point type");

public:
    using value_type = T;
    using descriptor_type = ParameterDescriptor<T>;
    using config_type = ParameterSmoothingConfig<T>;

public:
    /**
     * @brief Constructs an unbound parameter state.
     */
    constexpr ParameterState() noexcept = default;

    /**
     * @brief Constructs and binds to a descriptor.
     *
     * If the descriptor is invalid or null, the instance remains unbound.
     */
    explicit ParameterState(
        const descriptor_type* descriptor) noexcept
    {
        (void)bind(descriptor);
    }

    /**
     * @brief Binds this state to an immutable descriptor and resets to default.
     *
     * @return true when the descriptor is non-null and internally valid.
     */
    CVDSP_NODISCARD bool bind(
        const descriptor_type* descriptor) noexcept
    {
        if (descriptor == nullptr || !descriptor->isValid())
        {
            descriptor_ = nullptr;
            isBound_ = false;
            isPrepared_ = false;
            resetValues();
            return false;
        }

        descriptor_ = descriptor;
        isBound_ = true;
        (void)resetToDefault();
        return true;
    }

    /**
     * @brief Returns true when this state has a valid descriptor binding.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isBound() const noexcept
    {
        return isBound_ && descriptor_ != nullptr;
    }

    /**
     * @brief Returns true when smoothing has been prepared.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isPrepared() const noexcept
    {
        return isPrepared_;
    }

    /**
     * @brief Prepares all concrete smoothers with one shared configuration.
     */
    void prepare(
        const config_type& config) noexcept
    {
        config_ = sanitizeConfig(config);

        linear_.prepare(
            config_.sampleRate,
            config_.rampTimeSeconds);

        exponential_.prepare(
            config_.sampleRate,
            config_.rampTimeSeconds,
            config_.exponentialCurve);

        onePole_.prepare(
            config_.sampleRate,
            config_.onePoleTimeConstant);

        resetSmoothers(currentReal_);
        isPrepared_ = true;
        restoreActiveSmootherTarget();
    }

    /**
     * @brief Prepares all smoothers from scalar configuration values.
     */
    void prepare(
        const T sampleRate,
        const T rampTimeSeconds = static_cast<T>(0.005),
        const T exponentialCurve = static_cast<T>(5),
        const T onePoleTimeConstant = static_cast<T>(0.010)) noexcept
    {
        prepare(config_type{
            sampleRate,
            rampTimeSeconds,
            exponentialCurve,
            onePoleTimeConstant});
    }

    /**
     * @brief Selects the active smoothing mode.
     */
    void setSmoothingMode(
        const ParameterSmoothingMode mode) noexcept
    {
        smoothingMode_ = mode;
        resetSmoothers(currentReal_);
        restoreActiveSmootherTarget();
    }

    /**
     * @brief Returns the active smoothing mode.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE ParameterSmoothingMode getSmoothingMode() const noexcept
    {
        return smoothingMode_;
    }

    /**
     * @brief Updates the default linear/exponential ramp time and re-prepares smoothers.
     */
    void setRampTimeSeconds(
        const T rampTimeSeconds) noexcept
    {
        config_.rampTimeSeconds = nonNegativeOrZero(rampTimeSeconds);
        prepare(config_);
    }

    /**
     * @brief Updates the exponential curve and re-prepares smoothers.
     */
    void setExponentialCurve(
        const T curve) noexcept
    {
        config_.exponentialCurve = positiveOrDefault(curve, static_cast<T>(5));
        prepare(config_);
    }

    /**
     * @brief Updates the one-pole time constant and re-prepares smoothers.
     */
    void setOnePoleTimeConstant(
        const T timeConstantSeconds) noexcept
    {
        config_.onePoleTimeConstant = nonNegativeOrZero(timeConstantSeconds);
        prepare(config_);
    }

    /**
     * @brief Resets current and target values to descriptor default immediately.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool resetToDefault() noexcept
    {
        if (!isBound())
            return false;

        const T defaultReal = descriptor_->getDefaultValue();
        (void)setImmediateReal(defaultReal);
        clearDirty();
        return true;
    }

    /**
     * @brief Applies a normalized value immediately without smoothing.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool setImmediateNormalized(
        const T normalized) noexcept
    {
        if (!isBound())
            return false;

        const T safeNormalized = descriptor_->clampNormalized(normalized);
        const T real = descriptor_->normalizedToReal(safeNormalized);
        applyImmediate(real, descriptor_->realToNormalized(real));
        return true;
    }

    /**
     * @brief Applies a real value immediately without smoothing.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool setImmediateReal(
        const T real) noexcept
    {
        if (!isBound())
            return false;

        const T quantizedReal = descriptor_->quantizeReal(real);
        applyImmediate(quantizedReal, descriptor_->realToNormalized(quantizedReal));
        return true;
    }

    /**
     * @brief Sets a normalized target using the configured default ramp.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool setTargetNormalized(
        const T normalized) noexcept
    {
        if (!isBound())
            return false;

        const T safeNormalized = descriptor_->clampNormalized(normalized);
        const T real = descriptor_->normalizedToReal(safeNormalized);
        setTargetValues(real, descriptor_->realToNormalized(real), 0, false);
        return true;
    }

    /**
     * @brief Sets a normalized target using an explicit ramp length in samples.
     *
     * Linear and Exponential smoothers honor rampSamples. OnePole ignores the
     * sample count and uses its time constant.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool setTargetNormalized(
        const T normalized,
        const u32 rampSamples) noexcept
    {
        if (!isBound())
            return false;

        const T safeNormalized = descriptor_->clampNormalized(normalized);
        const T real = descriptor_->normalizedToReal(safeNormalized);
        setTargetValues(real, descriptor_->realToNormalized(real), rampSamples, true);
        return true;
    }

    /**
     * @brief Sets a real target using the configured default ramp.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool setTargetReal(
        const T real) noexcept
    {
        if (!isBound())
            return false;

        const T quantizedReal = descriptor_->quantizeReal(real);
        setTargetValues(quantizedReal, descriptor_->realToNormalized(quantizedReal), 0, false);
        return true;
    }

    /**
     * @brief Sets a real target using an explicit ramp length in samples.
     *
     * Linear and Exponential smoothers honor rampSamples. OnePole ignores the
     * sample count and uses its time constant.
     *
     * @return false when the state is not bound.
     */
    CVDSP_NODISCARD bool setTargetReal(
        const T real,
        const u32 rampSamples) noexcept
    {
        if (!isBound())
            return false;

        const T quantizedReal = descriptor_->quantizeReal(real);
        setTargetValues(quantizedReal, descriptor_->realToNormalized(quantizedReal), rampSamples, true);
        return true;
    }

    /**
     * @brief Advances the active smoother by one sample and returns the real value.
     */
    CVDSP_NODISCARD T processSample() noexcept
    {
        if (!isBound())
            return currentReal_;

        if (!shouldSmooth())
        {
            currentReal_ = targetReal_;
            currentNormalized_ = targetNormalized_;
            resetSmoothers(currentReal_);
            return currentReal_;
        }

        switch (smoothingMode_)
        {
            case ParameterSmoothingMode::Linear:
                currentReal_ = linear_.process();
                break;

            case ParameterSmoothingMode::Exponential:
                currentReal_ = exponential_.process();
                break;

            case ParameterSmoothingMode::OnePole:
                currentReal_ = onePole_.process();
                break;

            case ParameterSmoothingMode::None:
            default:
                currentReal_ = targetReal_;
                break;
        }

        if (!isSmoothing())
        {
            currentReal_ = targetReal_;
            currentNormalized_ = targetNormalized_;
        }

        return currentReal_;
    }

    /**
     * @brief Returns the bound descriptor, or nullptr when unbound.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE const descriptor_type* getDescriptor() const noexcept
    {
        return descriptor_;
    }

    /**
     * @brief Returns the descriptor ID, or zero when unbound.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE ParameterID getID() const noexcept
    {
        return isBound() ? descriptor_->getID() : ParameterID{};
    }

    /**
     * @brief Returns the current real value used by DSP processors.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentReal() const noexcept
    {
        return currentReal_;
    }

    /**
     * @brief Returns the current normalized value cache.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentNormalized() const noexcept
    {
        return currentNormalized_;
    }

    /**
     * @brief Returns the target real value.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetReal() const noexcept
    {
        return targetReal_;
    }

    /**
     * @brief Returns the target normalized value.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetNormalized() const noexcept
    {
        return targetNormalized_;
    }

    /**
     * @brief Returns true when the active smoother is currently transitioning.
     */
    CVDSP_NODISCARD bool isSmoothing() const noexcept
    {
        if (!shouldSmooth())
            return false;

        switch (smoothingMode_)
        {
            case ParameterSmoothingMode::Linear:
                return linear_.isSmoothing();

            case ParameterSmoothingMode::Exponential:
                return exponential_.isSmoothing();

            case ParameterSmoothingMode::OnePole:
                return currentReal_ != targetReal_;

            case ParameterSmoothingMode::None:
            default:
                return false;
        }
    }

    /**
     * @brief Returns true when the value changed since the dirty flag was cleared.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isDirty() const noexcept
    {
        return dirty_;
    }

    /**
     * @brief Clears the dirty flag without changing values.
     */
    CVDSP_FORCE_INLINE void clearDirty() noexcept
    {
        dirty_ = false;
    }

    /**
     * @brief Returns true when descriptor policy allows host automation.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isAutomatable() const noexcept
    {
        return isBound() && descriptor_->isAutomatable();
    }

    /**
     * @brief Returns true when descriptor policy allows modulation.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isModulatable() const noexcept
    {
        return isBound() && descriptor_->isModulatable();
    }

    /**
     * @brief Returns true when descriptor policy marks this parameter persistent.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isPersistent() const noexcept
    {
        return isBound() && descriptor_->isPersistent();
    }

    /**
     * @brief Returns true when the descriptor is Boolean.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isBoolean() const noexcept
    {
        return isBound() && descriptor_->getValueKind() == ParameterValueKind::Boolean;
    }

    /**
     * @brief Returns true when the descriptor is Enum.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isEnum() const noexcept
    {
        return isBound() && descriptor_->getValueKind() == ParameterValueKind::Enum;
    }

    /**
     * @brief Returns true when the descriptor is Discrete, Boolean, or Enum.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isDiscrete() const noexcept
    {
        return isBound()
            && descriptor_->getValueKind() != ParameterValueKind::Continuous;
    }

private:
    CVDSP_NODISCARD static CVDSP_FORCE_INLINE config_type sanitizeConfig(
        const config_type& config) noexcept
    {
        return config_type{
            positiveOrDefault(config.sampleRate, static_cast<T>(44100)),
            nonNegativeOrZero(config.rampTimeSeconds),
            positiveOrDefault(config.exponentialCurve, static_cast<T>(5)),
            nonNegativeOrZero(config.onePoleTimeConstant)};
    }

    CVDSP_NODISCARD static CVDSP_FORCE_INLINE T positiveOrDefault(
        const T value,
        const T fallback) noexcept
    {
        return value > static_cast<T>(0) ? value : fallback;
    }

    CVDSP_NODISCARD static CVDSP_FORCE_INLINE T nonNegativeOrZero(
        const T value) noexcept
    {
        return value > static_cast<T>(0) ? value : static_cast<T>(0);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool shouldSmooth() const noexcept
    {
        return isPrepared_
            && smoothingMode_ != ParameterSmoothingMode::None
            && isBound()
            && descriptor_->getValueKind() == ParameterValueKind::Continuous;
    }

    void resetValues() noexcept
    {
        currentReal_ = static_cast<T>(0);
        targetReal_ = static_cast<T>(0);
        currentNormalized_ = static_cast<T>(0);
        targetNormalized_ = static_cast<T>(0);
        dirty_ = false;
    }

    void resetSmoothers(
        const T value) noexcept
    {
        linear_.reset(value);
        exponential_.reset(value);
        onePole_.reset(value);
    }

    void restoreActiveSmootherTarget() noexcept
    {
        if (!shouldSmooth() || currentReal_ == targetReal_)
            return;

        switch (smoothingMode_)
        {
            case ParameterSmoothingMode::Linear:
                linear_.setTarget(targetReal_);
                break;

            case ParameterSmoothingMode::Exponential:
                exponential_.setTarget(targetReal_);
                break;

            case ParameterSmoothingMode::OnePole:
                onePole_.setTarget(targetReal_);
                break;

            case ParameterSmoothingMode::None:
            default:
                break;
        }
    }

    void applyImmediate(
        const T real,
        const T normalized) noexcept
    {
        currentReal_ = real;
        targetReal_ = real;
        currentNormalized_ = normalized;
        targetNormalized_ = normalized;
        resetSmoothers(real);
        dirty_ = true;
    }

    void setTargetValues(
        const T real,
        const T normalized,
        const u32 rampSamples,
        const bool useExplicitRamp) noexcept
    {
        targetReal_ = real;
        targetNormalized_ = normalized;
        dirty_ = true;

        if (!shouldSmooth())
        {
            applyImmediate(real, normalized);
            return;
        }

        switch (smoothingMode_)
        {
            case ParameterSmoothingMode::Linear:
                if (useExplicitRamp)
                    linear_.setTarget(real, rampSamples);
                else
                    linear_.setTarget(real);
                break;

            case ParameterSmoothingMode::Exponential:
                if (useExplicitRamp)
                    exponential_.setTarget(real, rampSamples);
                else
                    exponential_.setTarget(real);
                break;

            case ParameterSmoothingMode::OnePole:
                onePole_.setTarget(real);
                break;

            case ParameterSmoothingMode::None:
            default:
                applyImmediate(real, normalized);
                break;
        }
    }

private:
    const descriptor_type* descriptor_ = nullptr;
    config_type config_{};
    ParameterSmoothingMode smoothingMode_ = ParameterSmoothingMode::None;

    LinearSmoother<T> linear_{};
    ExponentialSmoother<T> exponential_{};
    OnePoleSmoother<T> onePole_{};

    T currentReal_ = static_cast<T>(0);
    T targetReal_ = static_cast<T>(0);
    T currentNormalized_ = static_cast<T>(0);
    T targetNormalized_ = static_cast<T>(0);

    bool isBound_ = false;
    bool isPrepared_ = false;
    bool dirty_ = false;
};

} // namespace cvdsp::manager

#endif // CVDSP_MANAGER_PARAMETERSTATE_HPP
