#ifndef CVDSP_GUITAR_PEDALS_SUSTAINERDSP_HPP
#define CVDSP_GUITAR_PEDALS_SUSTAINERDSP_HPP

/**
 * @file SustainerDSP.hpp
 * @brief Guitar sustainer pedal with upward sustain gain and integrated noise gate.
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
#include "../../Dynamics/EnvelopeFollower.hpp"
#include "../../Dynamics/NoiseGate.hpp"
#include "../../Filters/Biquad.hpp"
#include "../../Manager/ParameterDescriptor.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief High-sustain guitar compressor/leveler with a gated safety path.
 *
 * The class is intentionally independent from PedalDriveCore because its topology
 * is linear dynamics rather than nonlinear clipping. It combines a sidechain high
 * pass, envelope follower, capped upward gain computer, smoothed gain application,
 * output level and dry/wet mix.
 */
template<typename T = float>
class SustainerDSP
{
    static_assert(std::is_floating_point_v<T>, "SustainerDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 13>;

    constexpr SustainerDSP() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;

        inputGain_.prepare(sampleRate_);
        outputGain_.prepare(sampleRate_);
        mix_.prepare(sampleRate_);

        envelope_.prepare(sampleRate_);
        gate_.prepare(sampleRate_);
        gainSmoother_.prepare(sampleRate_, static_cast<T>(0.020));

        sidechainHighPass_.prepare(sampleRate_);
        configureSidechainHighPass();

        applyDefaultParameters();
        reset();
    }

    void reset() noexcept
    {
        inputGain_.reset();
        outputGain_.reset();
        mix_.reset();
        envelope_.reset();
        gate_.reset();
        sidechainHighPass_.reset();
        gainSmoother_.reset(static_cast<T>(1));
        currentGainDb_ = static_cast<T>(0);
    }

    void setBypassed(bool enabled) noexcept { bypassed_ = enabled; }
    void setInputGainDb(T db) noexcept { inputGain_.setGainDb(db); }
    void setLevelDb(T db) noexcept { outputGain_.setGainDb(db); }
    void setMix(T normalized) noexcept { mix_.setMix(normalized); }

    void setSustain(T normalized) noexcept
    {
        sustain_ = clamp01(normalized);
        thresholdDb_ = normalizedToLinear(static_cast<T>(-34), static_cast<T>(-62), sustain_);
        ratio_ = normalizedToLinear(static_cast<T>(3), static_cast<T>(18), sustain_);
        makeupGainDb_ = normalizedToLinear(static_cast<T>(2), static_cast<T>(18), sustain_);
        maxBoostDb_ = normalizedToLinear(static_cast<T>(10), static_cast<T>(32), sustain_);
    }

    void setAttackMs(T milliseconds) noexcept
    {
        attackMs_ = std::clamp(milliseconds, static_cast<T>(0.2), static_cast<T>(50));
        envelope_.setAttackMs(attackMs_);
    }

    void setReleaseMs(T milliseconds) noexcept
    {
        releaseMs_ = std::clamp(milliseconds, static_cast<T>(50), static_cast<T>(2000));
        envelope_.setReleaseMs(releaseMs_);
    }

    void setDetectionMode(dynamics::EnvelopeMode mode) noexcept
    {
        envelope_.setMode(mode);
    }

    void setGateEnabled(bool enabled) noexcept { gateEnabled_ = enabled; }

    void setGateThresholdDb(T thresholdDb) noexcept
    {
        gateThresholdDb_ = std::clamp(thresholdDb, static_cast<T>(-90), static_cast<T>(-20));
        gate_.setThresholdOpenDB(gateThresholdDb_);
        gate_.setThresholdCloseDB(gateThresholdDb_ - static_cast<T>(6));
    }

    void setGateReleaseMs(T milliseconds) noexcept
    {
        gateReleaseMs_ = std::clamp(milliseconds, static_cast<T>(5), static_cast<T>(500));
        gate_.setReleaseMs(gateReleaseMs_);
    }

    void setSidechainHighPassHz(T hz) noexcept
    {
        sidechainHighPassHz_ = std::clamp(hz, static_cast<T>(20), static_cast<T>(400));
        configureSidechainHighPass();
    }

    void setThresholdDb(T thresholdDb) noexcept
    {
        thresholdDb_ = std::clamp(thresholdDb, static_cast<T>(-80), static_cast<T>(-12));
    }

    void setRatio(T ratio) noexcept
    {
        ratio_ = std::clamp(ratio, static_cast<T>(1), static_cast<T>(40));
    }

    void setMakeupGainDb(T db) noexcept
    {
        makeupGainDb_ = std::clamp(db, static_cast<T>(-12), static_cast<T>(30));
    }

    void setMaxBoostDb(T db) noexcept
    {
        maxBoostDb_ = std::clamp(db, static_cast<T>(0), static_cast<T>(36));
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        if (bypassed_)
            return input;

        const T dry = input;
        const T levelInput = inputGain_.processSample(input);
        const T sidechain = sidechainHighPass_.process(levelInput);
        const T envelope = envelope_.process(sidechain);
        const T inputDb = linearToDb(envelope);
        const T targetGainDb = computeGainDb(inputDb);
        const T targetGain = decibelsToGain(targetGainDb);

        gainSmoother_.setTarget(targetGain);
        currentGainDb_ = targetGainDb;

        const T gated = gateEnabled_ ? gate_.process(levelInput) : levelInput;
        T wet = gated * gainSmoother_.process();
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

    [[nodiscard]] T getCurrentGainDb() const noexcept { return currentGainDb_; }
    [[nodiscard]] T getSustain() const noexcept { return sustain_; }
    [[nodiscard]] T getThresholdDb() const noexcept { return thresholdDb_; }
    [[nodiscard]] T getRatio() const noexcept { return ratio_; }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassed_; }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "sustainer.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "sustainer.input_gain", "Global"),
            makePercent(PedalParameterIDs::Sustain, "Sustain", "Sustain", 0.55, "sustainer.sustain", "Dynamics"),
            makeMilliseconds(PedalParameterIDs::Attack, "Attack", "Envelope Attack", 0.2, 50.0, 5.0, "sustainer.attack", "Dynamics"),
            makeMilliseconds(PedalParameterIDs::Release, "Release", "Envelope Release", 50.0, 2000.0, 750.0, "sustainer.release", "Dynamics"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, 0.0, "sustainer.level", "Global"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "sustainer.mix", "Global"),
            makeBoolean(PedalParameterIDs::GateEnable, "Gate", "Gate Enable", true, "sustainer.gate_enable", kAutomatablePersistent),
            makeDecibel(PedalParameterIDs::GateThreshold, "Gate Th", "Gate Threshold", -90.0, -20.0, -58.0, "sustainer.gate_threshold", "Gate"),
            makeMilliseconds(PedalParameterIDs::GateRelease, "Gate Rel", "Gate Release", 5.0, 500.0, 80.0, "sustainer.gate_release", "Gate"),
            makeFrequency(PedalParameterIDs::SidechainHighPassFrequency, "SC HPF", "Sidechain High-Pass", 20.0, 400.0, 80.0, "sustainer.sidechain_hpf", "Sidechain"),
            makeEnum(PedalParameterIDs::DetectorMode, "Detect", "Detector Mode", kDetectorEntries.data(), kDetectorEntries.size(), 0.0, "sustainer.detector_mode", "Sidechain"),
            makeDecibel(PedalParameterIDs::MaxBoost, "Max Boost", "Maximum Sustain Boost", 0.0, 36.0, 22.0, "sustainer.max_boost", "Advanced")
        }};
        return descriptors;
    }

private:
    static constexpr manager::ParameterFlags kAutomatablePersistent =
        manager::toMask(manager::ParameterFlag::Automatable) |
        manager::toMask(manager::ParameterFlag::Persistent);

    static constexpr std::array<manager::ParameterEnumEntry, 2> kDetectorEntries {{
        { 0u, "Peak" }, { 1u, "RMS" }
    }};

    void applyDefaultParameters() noexcept
    {
        setBypassed(false);
        setInputGainDb(static_cast<T>(0));
        setSustain(static_cast<T>(0.55));
        setAttackMs(static_cast<T>(5));
        setReleaseMs(static_cast<T>(750));
        setDetectionMode(dynamics::EnvelopeMode::Peak);
        setGateEnabled(true);
        setGateThresholdDb(static_cast<T>(-58));
        setGateReleaseMs(static_cast<T>(80));
        setSidechainHighPassHz(static_cast<T>(80));
        setLevelDb(static_cast<T>(0));
        setMix(static_cast<T>(1));
    }

    void configureSidechainHighPass() noexcept
    {
        sidechainHighPass_.setType(filters::BiquadType::HighPass);
        sidechainHighPass_.setFrequency(sidechainHighPassHz_);
        sidechainHighPass_.setQ(PedalConstants<T>::kDefaultQ);
        sidechainHighPass_.updateCoefficients();
    }

    [[nodiscard]] inline T computeGainDb(T inputDb) const noexcept
    {
        T gainDb = makeupGainDb_;

        if (inputDb < thresholdDb_)
        {
            const T belowThresholdDb = thresholdDb_ - inputDb;
            const T upwardSlope = static_cast<T>(1) - (static_cast<T>(1) / ratio_);
            gainDb += belowThresholdDb * upwardSlope;
        }
        else
        {
            const T aboveThresholdDb = inputDb - thresholdDb_;
            gainDb -= aboveThresholdDb * static_cast<T>(0.15);
        }

        return std::clamp(gainDb, static_cast<T>(-12), maxBoostDb_);
    }

    [[nodiscard]] static inline T linearToDb(T value) noexcept
    {
        constexpr T kFloor = static_cast<T>(1e-7);
        return static_cast<T>(20) * std::log10(std::max(std::abs(value), kFloor));
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

    static Descriptor makeMilliseconds(
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
            manager::ParameterUnit::Milliseconds,
            manager::ParameterScale::Linear,
            kAutomatablePersistent,
            { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr,
            0,
            stableTextID,
            "ms",
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

    static Descriptor makeEnum(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        const manager::ParameterEnumEntry* entries,
        std::size_t entryCount,
        double defaultValue,
        const char* stableTextID,
        const char* groupName) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::Index,
            manager::ParameterScale::Enum,
            kAutomatablePersistent,
            { static_cast<T>(0), static_cast<T>(entryCount > 0 ? entryCount - 1 : 1), static_cast<T>(defaultValue), static_cast<T>(1) },
            entries,
            entryCount,
            stableTextID,
            nullptr,
            groupName,
            0);
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    bool bypassed_ { false };
    bool gateEnabled_ { true };

    T sustain_ { static_cast<T>(0.55) };
    T thresholdDb_ { static_cast<T>(-48) };
    T ratio_ { static_cast<T>(8) };
    T makeupGainDb_ { static_cast<T>(10) };
    T maxBoostDb_ { static_cast<T>(22) };
    T attackMs_ { static_cast<T>(5) };
    T releaseMs_ { static_cast<T>(750) };
    T gateThresholdDb_ { static_cast<T>(-58) };
    T gateReleaseMs_ { static_cast<T>(80) };
    T sidechainHighPassHz_ { static_cast<T>(80) };
    T currentGainDb_ { static_cast<T>(0) };

    PedalGainStage<T> inputGain_ {};
    PedalGainStage<T> outputGain_ {};
    PedalMix<T> mix_ {};
    dynamics::EnvelopeFollower<T> envelope_ {};
    dynamics::NoiseGate<T> gate_ {};
    filters::Biquad<T> sidechainHighPass_ {};
    OnePoleSmoother<T> gainSmoother_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_SUSTAINERDSP_HPP
