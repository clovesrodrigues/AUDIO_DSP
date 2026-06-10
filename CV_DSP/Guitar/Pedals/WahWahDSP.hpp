#ifndef CVDSP_GUITAR_PEDALS_WAHWAHDSP_HPP
#define CVDSP_GUITAR_PEDALS_WAHWAHDSP_HPP

/**
 * @file WahWahDSP.hpp
 * @brief Expression-controlled guitar wah-wah pedal built on a TPT state-variable filter.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "PedalGainStage.hpp"
#include "PedalMix.hpp"
#include "PedalParameterIDs.hpp"
#include "PedalParameterUtils.hpp"
#include "../../Core/AudioBufferView.hpp"
#include "../../Core/ParameterSmoother.hpp"
#include "../../Filters/StateVariableFilter.hpp"
#include "../../Manager/ParameterDescriptor.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief Manual/external-expression wah-wah with dynamic frequency and Q mapping.
 *
 * The pedal maps a normalized expression value to a resonant band-pass SVF. The
 * expression is smoothed to avoid zipper noise, then shaped by a taper control so
 * host automation, MIDI expression pedals and future intelligent controllers can
 * feed the same stable API.
 */
template<typename T = float>
class WahWahDSP
{
    static_assert(std::is_floating_point_v<T>, "WahWahDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 13>;

    constexpr WahWahDSP() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;

        inputGain_.prepare(sampleRate_);
        outputGain_.prepare(sampleRate_);
        mix_.prepare(sampleRate_);
        expressionSmoother_.prepare(sampleRate_, static_cast<T>(0.015));

        filter_.prepare(sampleRate_, filters::SVFMode::BandPass);

        applyDefaultParameters();
        reset();
    }

    void reset() noexcept
    {
        inputGain_.reset();
        outputGain_.reset();
        mix_.reset();
        expressionSmoother_.reset(expression_);
        filter_.reset();
        currentCutoffHz_ = mapExpressionToFrequency(expression_);
        currentQ_ = mapExpressionToQ(expression_);
    }

    void setBypassed(bool enabled) noexcept { bypassed_ = enabled; }
    void setInputGainDb(T db) noexcept { inputGain_.setGainDb(db); }
    void setLevelDb(T db) noexcept { outputGain_.setGainDb(db); }
    void setMix(T normalized) noexcept { mix_.setMix(normalized); }

    void setExpression(T normalized) noexcept
    {
        expression_ = clamp01(normalized);
        expressionSmoother_.setTarget(expression_);
    }

    void setMinFrequencyHz(T hz) noexcept
    {
        minFrequencyHz_ = clampFrequency(hz);
        if (maxFrequencyHz_ < minFrequencyHz_)
            maxFrequencyHz_ = minFrequencyHz_;
    }

    void setMaxFrequencyHz(T hz) noexcept
    {
        maxFrequencyHz_ = clampFrequency(hz);
        if (minFrequencyHz_ > maxFrequencyHz_)
            minFrequencyHz_ = maxFrequencyHz_;
    }

    void setMinQ(T q) noexcept
    {
        minQ_ = std::clamp(q, static_cast<T>(0.2), static_cast<T>(12));
        if (maxQ_ < minQ_)
            maxQ_ = minQ_;
    }

    void setMaxQ(T q) noexcept
    {
        maxQ_ = std::clamp(q, static_cast<T>(0.2), static_cast<T>(12));
        if (minQ_ > maxQ_)
            minQ_ = maxQ_;
    }

    void setTaper(T normalized) noexcept
    {
        taper_ = clamp01(normalized);
    }

    void setBandPassGain(T gain) noexcept
    {
        bandPassGain_ = std::clamp(gain, static_cast<T>(0), static_cast<T>(4));
    }

    void setDryGain(T gain) noexcept
    {
        dryGain_ = std::clamp(gain, static_cast<T>(0), static_cast<T>(1));
    }

    void setDrive(T normalized) noexcept
    {
        drive_ = clamp01(normalized);
        driveAmount_ = normalizedToLinear(static_cast<T>(1), static_cast<T>(2.5), drive_);
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        if (bypassed_)
            return input;

        const T dry = input;
        const T drivenInput = inputGain_.processSample(input);
        const T smoothExpression = expressionSmoother_.process();
        const T shapedExpression = shapeExpression(smoothExpression);

        currentCutoffHz_ = mapExpressionToFrequency(shapedExpression);
        currentQ_ = mapExpressionToQ(shapedExpression);

        filter_.setCutoff(currentCutoffHz_);
        filter_.setResonance(currentQ_);

        T band = filter_.process(drivenInput) * bandPassGain_;
        band = saturateBandPass(band);

        T wet = dryGain_ * drivenInput + band;
        wet = outputGain_.processSample(wet);
        return mix_.processSample(dry, wet);
    }

    void processBlock(AudioBufferView<T> buffer) noexcept
    {
        if (!buffer.isValid())
            return;

        for (typename AudioBufferView<T>::size_type channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            T* channelData = buffer.getChannel(channel);
            for (typename AudioBufferView<T>::size_type sample = 0; sample < buffer.getNumSamples(); ++sample)
                channelData[sample] = processSample(channelData[sample]);
        }
    }

    [[nodiscard]] T getExpression() const noexcept { return expression_; }
    [[nodiscard]] T getCurrentCutoffHz() const noexcept { return currentCutoffHz_; }
    [[nodiscard]] T getCurrentQ() const noexcept { return currentQ_; }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassed_; }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "wah_wah.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "wah_wah.input_gain", "Global"),
            makePercent(PedalParameterIDs::Expression, "Pedal", "Expression Pedal", 0.0, "wah_wah.expression", "Expression"),
            makeFrequency(PedalParameterIDs::MinFrequency, "Min Freq", "Minimum Frequency", 120.0, 1000.0, 250.0, "wah_wah.min_frequency", "Filter"),
            makeFrequency(PedalParameterIDs::MaxFrequency, "Max Freq", "Maximum Frequency", 800.0, 4000.0, 2200.0, "wah_wah.max_frequency", "Filter"),
            makeLinear(PedalParameterIDs::MinQ, "Min Q", "Minimum Q", 0.2, 8.0, 2.0, "wah_wah.min_q", "Filter"),
            makeLinear(PedalParameterIDs::MaxQ, "Max Q", "Maximum Q", 0.2, 12.0, 5.5, "wah_wah.max_q", "Filter"),
            makePercent(PedalParameterIDs::Taper, "Taper", "Expression Taper", 0.65, "wah_wah.taper", "Expression"),
            makeLinear(PedalParameterIDs::BandPassGain, "BP Gain", "Band-Pass Gain", 0.0, 4.0, 2.0, "wah_wah.bandpass_gain", "Filter"),
            makeLinear(PedalParameterIDs::DryGain, "Dry Gain", "Dry Body Gain", 0.0, 1.0, 0.2, "wah_wah.dry_gain", "Filter"),
            makePercent(PedalParameterIDs::FilterDrive, "Drive", "Vintage Filter Drive", 0.12, "wah_wah.drive", "Voice"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, 0.0, "wah_wah.level", "Global"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "wah_wah.mix", "Global")
        }};
        return descriptors;
    }

private:
    static constexpr manager::ParameterFlags kAutomatablePersistent =
        manager::toMask(manager::ParameterFlag::Automatable) |
        manager::toMask(manager::ParameterFlag::Persistent);

    void applyDefaultParameters() noexcept
    {
        setBypassed(false);
        setInputGainDb(static_cast<T>(0));
        setExpression(static_cast<T>(0));
        setMinFrequencyHz(static_cast<T>(250));
        setMaxFrequencyHz(static_cast<T>(2200));
        setMinQ(static_cast<T>(2));
        setMaxQ(static_cast<T>(5.5));
        setTaper(static_cast<T>(0.65));
        setBandPassGain(static_cast<T>(2));
        setDryGain(static_cast<T>(0.2));
        setDrive(static_cast<T>(0.12));
        setLevelDb(static_cast<T>(0));
        setMix(static_cast<T>(1));
    }

    [[nodiscard]] T clampFrequency(T hz) const noexcept
    {
        const T nyquistSafe = sampleRate_ * static_cast<T>(0.45);
        return std::clamp(hz, static_cast<T>(20), std::min(static_cast<T>(5000), nyquistSafe));
    }

    [[nodiscard]] T shapeExpression(T expression) const noexcept
    {
        const T x = clamp01(expression);
        const T cubic = x * x * x;
        const T logLike = x * x;
        return std::clamp(normalizedToLinear(logLike, cubic, taper_), static_cast<T>(0), static_cast<T>(1));
    }

    [[nodiscard]] T mapExpressionToFrequency(T expression) const noexcept
    {
        const T safeMin = std::max(static_cast<T>(20), std::min(minFrequencyHz_, maxFrequencyHz_));
        const T safeMax = std::max(safeMin, std::max(minFrequencyHz_, maxFrequencyHz_));
        return normalizedToLogFrequency(safeMin, safeMax, expression);
    }

    [[nodiscard]] T mapExpressionToQ(T expression) const noexcept
    {
        return normalizedToLinear(minQ_, maxQ_, clamp01(expression));
    }

    [[nodiscard]] T saturateBandPass(T sample) const noexcept
    {
        if (drive_ <= static_cast<T>(0))
            return sample;

        const T driven = sample * driveAmount_;
        const T saturated = std::tanh(driven);
        const T blend = drive_ * static_cast<T>(0.65);
        return sample * (static_cast<T>(1) - blend) + saturated * blend;
    }

    static Descriptor makeLinear(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        double minimum,
        double maximum,
        double defaultValue,
        const char* stableTextID,
        const char* groupName) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::None,
            manager::ParameterScale::Linear,
            kAutomatablePersistent,
            { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr,
            0,
            stableTextID,
            nullptr,
            groupName,
            2);
    }

    static Descriptor makePercent(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        double defaultValue,
        const char* stableTextID,
        const char* groupName) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::Percent,
            manager::ParameterScale::Percentage,
            kAutomatablePersistent,
            { static_cast<T>(0), static_cast<T>(1), static_cast<T>(defaultValue) },
            nullptr,
            0,
            stableTextID,
            "%",
            groupName,
            1);
    }

    static Descriptor makeBoolean(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        bool defaultValue,
        const char* stableTextID,
        manager::ParameterFlags flags) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::None,
            manager::ParameterScale::Boolean,
            flags | manager::toMask(manager::ParameterFlag::Persistent),
            { static_cast<T>(0), static_cast<T>(1), defaultValue ? static_cast<T>(1) : static_cast<T>(0), static_cast<T>(1) },
            nullptr,
            0,
            stableTextID,
            nullptr,
            "Global",
            0);
    }

    static Descriptor makeDecibel(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        double minimum,
        double maximum,
        double defaultValue,
        const char* stableTextID,
        const char* groupName) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::Decibels,
            manager::ParameterScale::Decibel,
            kAutomatablePersistent,
            { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr,
            0,
            stableTextID,
            "dB",
            groupName,
            1);
    }

    static Descriptor makeFrequency(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        double minimum,
        double maximum,
        double defaultValue,
        const char* stableTextID,
        const char* groupName) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::Hertz,
            manager::ParameterScale::Logarithmic,
            kAutomatablePersistent,
            { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr,
            0,
            stableTextID,
            "Hz",
            groupName,
            1);
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    bool bypassed_ { false };

    T expression_ { static_cast<T>(0) };
    T minFrequencyHz_ { static_cast<T>(250) };
    T maxFrequencyHz_ { static_cast<T>(2200) };
    T minQ_ { static_cast<T>(2) };
    T maxQ_ { static_cast<T>(5.5) };
    T taper_ { static_cast<T>(0.65) };
    T bandPassGain_ { static_cast<T>(2) };
    T dryGain_ { static_cast<T>(0.2) };
    T drive_ { static_cast<T>(0.12) };
    T driveAmount_ { static_cast<T>(1.18) };
    T currentCutoffHz_ { static_cast<T>(250) };
    T currentQ_ { static_cast<T>(2) };

    PedalGainStage<T> inputGain_ {};
    PedalGainStage<T> outputGain_ {};
    PedalMix<T> mix_ {};
    OnePoleSmoother<T> expressionSmoother_ {};
    filters::StateVariableFilter<T> filter_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_WAHWAHDSP_HPP
