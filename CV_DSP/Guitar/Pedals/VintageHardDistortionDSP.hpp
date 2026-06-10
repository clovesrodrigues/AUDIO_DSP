#ifndef CVDSP_GUITAR_PEDALS_VINTAGEHARDDISTORTIONDSP_HPP
#define CVDSP_GUITAR_PEDALS_VINTAGEHARDDISTORTIONDSP_HPP

/**
 * @file VintageHardDistortionDSP.hpp
 * @brief Aggressive hard-clipping distortion with scooped post-EQ voicing.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

#include "PedalDriveCore.hpp"
#include "PedalParameterIDs.hpp"
#include "../../Manager/ParameterDescriptor.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief Vintage hard distortion inspired by classic orange-box distortion pedals.
 *
 * This pedal keeps more low end before clipping than ClassicOverdriveDSP, uses a
 * hard clipper by default, and carves the post-distortion spectrum with a
 * controllable mid scoop, high bite and fizz cut.
 */
template<typename T = float>
class VintageHardDistortionDSP
{
    static_assert(std::is_floating_point_v<T>, "VintageHardDistortionDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 19>;

    constexpr VintageHardDistortionDSP() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        core_.prepare(sampleRate_);
        applyDefaultParameters();
        core_.reset();
    }

    void reset() noexcept
    {
        core_.reset();
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        return core_.processSample(input);
    }

    void processBlock(AudioBufferView<T> buffer) noexcept
    {
        core_.processBlock(buffer);
    }

    void setBypassed(bool enabled) noexcept { core_.setBypassed(enabled); }
    void setDistortion(T normalized) noexcept { core_.setDriveDb(normalizedToDecibels(static_cast<T>(6), static_cast<T>(54), normalized)); }
    void setTone(T normalized) noexcept
    {
        const T tone = clamp01(normalized);
        core_.postFilter().setTone(tone);
        core_.postFilter().setLowPassFrequency(normalizedToLogFrequency(static_cast<T>(1800), static_cast<T>(9000), tone));
        core_.postFilter().setPresenceGainDb(normalizedToLinear(static_cast<T>(-4), static_cast<T>(7), tone));
        setHighBite(tone);
    }
    void setLevelDb(T db) noexcept { core_.setOutputGainDb(db); }
    void setInputGainDb(T db) noexcept { core_.setInputGainDb(db); }
    void setMix(T normalized) noexcept { core_.setDryWetMix(normalized); }
    void setOversamplingMode(PedalOversamplingMode mode) noexcept { core_.setOversamplingMode(mode); }
    void setQualityMode(PedalQualityMode mode) noexcept { core_.setQualityMode(mode); }
    void setPreHighPassHz(T hz) noexcept { core_.preFilter().setHighPassFrequency(hz); }

    void setClipThreshold(T threshold) noexcept
    {
        const T clamped = std::clamp(threshold, PedalConstants<T>::kMinClipThreshold, PedalConstants<T>::kMaxClipThreshold);
        core_.clipper().setPositiveThreshold(clamped);
        core_.clipper().setNegativeThreshold(clamped);
    }
    void setPositiveThreshold(T threshold) noexcept { core_.clipper().setPositiveThreshold(threshold); }
    void setNegativeThreshold(T threshold) noexcept { core_.clipper().setNegativeThreshold(threshold); }
    void setThresholdLink(bool enabled) noexcept { core_.clipper().setThresholdLink(enabled); }
    void setAsymmetry(T normalized) noexcept { core_.clipper().setAsymmetry(normalized); }

    void setScoopAmount(T normalized) noexcept
    {
        scoopAmount_ = clamp01(normalized);
        core_.postFilter().setNotchDepthDb(normalizedToLinear(static_cast<T>(0), static_cast<T>(-18), scoopAmount_));
        core_.postFilter().setMiddleGainDb(normalizedToLinear(static_cast<T>(0), static_cast<T>(-9), scoopAmount_));
    }

    void setScoopFrequencyHz(T hz) noexcept
    {
        scoopFrequencyHz_ = hz;
        core_.postFilter().setNotchFrequency(hz);
        core_.postFilter().setMidFrequency(hz);
    }

    void setScoopQ(T q) noexcept
    {
        scoopQ_ = std::clamp(q, PedalConstants<T>::kMinQ, PedalConstants<T>::kMaxQ);
        core_.postFilter().setNotchQ(scoopQ_);
        core_.postFilter().setMidQ(scoopQ_);
    }

    void setFizzCutHz(T hz) noexcept { core_.postFilter().setFizzCutFrequency(hz); }
    void setHighBite(T normalized) noexcept
    {
        highBite_ = clamp01(normalized);
        core_.postFilter().setTrebleGainDb(normalizedToLinear(static_cast<T>(-3), static_cast<T>(5), highBite_));
        core_.postFilter().setPresenceGainDb(normalizedToLinear(static_cast<T>(-2), static_cast<T>(8), highBite_));
    }

    [[nodiscard]] PedalDriveCore<T>& core() noexcept { return core_; }
    [[nodiscard]] const PedalDriveCore<T>& core() const noexcept { return core_; }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "vintage_hard_distortion.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "vintage_hard_distortion.input_gain", "Global"),
            makePercent(PedalParameterIDs::Drive, "Dist", "Distortion", 0.65, "vintage_hard_distortion.distortion", "Drive"),
            makePercent(PedalParameterIDs::Tone, "Tone", "Tone", 0.55, "vintage_hard_distortion.tone", "Tone"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, 0.0, "vintage_hard_distortion.level", "Global"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "vintage_hard_distortion.mix", "Global"),
            makeEnum(PedalParameterIDs::Oversampling, "OS", "Oversampling", kOversamplingEntries.data(), kOversamplingEntries.size(), 2.0, "vintage_hard_distortion.oversampling", "Advanced"),
            makeEnum(PedalParameterIDs::QualityMode, "Quality", "Quality Mode", kQualityEntries.data(), kQualityEntries.size(), 1.0, "vintage_hard_distortion.quality", "Advanced"),
            makeFrequency(PedalParameterIDs::PreHighPassFrequency, "Pre HPF", "Pre High-Pass", 40.0, 500.0, 150.0, "vintage_hard_distortion.pre_hpf", "Voice"),
            makeLinear(PedalParameterIDs::PositiveThreshold, "+Thresh", "Positive Threshold", 0.1, 1.5, 0.6, "vintage_hard_distortion.positive_threshold", "Clip"),
            makeLinear(PedalParameterIDs::NegativeThreshold, "-Thresh", "Negative Threshold", 0.1, 1.5, 0.6, "vintage_hard_distortion.negative_threshold", "Clip"),
            makeBoolean(PedalParameterIDs::ThresholdLink, "Link", "Threshold Link", true, "vintage_hard_distortion.threshold_link"),
            makePercent(PedalParameterIDs::Asymmetry, "Asym", "Asymmetry", 0.0, "vintage_hard_distortion.asymmetry", "Clip"),
            makePercent(PedalParameterIDs::NotchDepth, "Scoop", "Scoop Amount", 0.5, "vintage_hard_distortion.scoop_amount", "Tone"),
            makeFrequency(PedalParameterIDs::NotchFrequency, "Scoop F", "Scoop Frequency", 200.0, 1000.0, 400.0, "vintage_hard_distortion.scoop_frequency", "Tone"),
            makeLinear(PedalParameterIDs::NotchQ, "Scoop Q", "Scoop Q", 0.3, 4.0, 1.2, "vintage_hard_distortion.scoop_q", "Tone"),
            makePercent(PedalParameterIDs::Presence, "Bite", "High Bite", 0.55, "vintage_hard_distortion.high_bite", "Tone"),
            makeFrequency(PedalParameterIDs::FizzCutFrequency, "Fizz", "Fizz Cut", 3000.0, 14000.0, 7000.0, "vintage_hard_distortion.fizz_cut", "Tone"),
            makeFrequency(PedalParameterIDs::PostLowPassFrequency, "LPF", "Post Low-Pass", 2000.0, 12000.0, 6000.0, "vintage_hard_distortion.post_lpf", "Tone")
        }};
        return descriptors;
    }

private:
    static constexpr manager::ParameterFlags kAutomatablePersistent =
        manager::toMask(manager::ParameterFlag::Automatable) |
        manager::toMask(manager::ParameterFlag::Persistent);

    static constexpr std::array<manager::ParameterEnumEntry, 4> kOversamplingEntries {{
        { 0u, "Off" }, { 1u, "2x" }, { 2u, "4x" }, { 3u, "8x" }
    }};

    static constexpr std::array<manager::ParameterEnumEntry, 3> kQualityEntries {{
        { 0u, "Eco" }, { 1u, "Normal" }, { 2u, "Studio" }
    }};

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
        return Descriptor(id, shortName, longName, manager::ParameterUnit::None, manager::ParameterScale::Linear,
            kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, nullptr, groupName, 2);
    }

    static Descriptor makePercent(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        double defaultValue,
        const char* stableTextID,
        const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Percent, manager::ParameterScale::Percentage,
            kAutomatablePersistent, { static_cast<T>(0), static_cast<T>(1), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, "%", groupName, 1);
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
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Decibels, manager::ParameterScale::Decibel,
            kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, "dB", groupName, 1);
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
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Hertz, manager::ParameterScale::Logarithmic,
            kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, "Hz", groupName, 1);
    }

    static Descriptor makeBoolean(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        bool defaultValue,
        const char* stableTextID,
        manager::ParameterFlags extraFlags = 0u) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::None, manager::ParameterScale::Boolean,
            kAutomatablePersistent | extraFlags,
            { static_cast<T>(0), static_cast<T>(1), defaultValue ? static_cast<T>(1) : static_cast<T>(0), static_cast<T>(1) },
            nullptr, 0, stableTextID, nullptr, "Global", 0);
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
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Index, manager::ParameterScale::Enum,
            kAutomatablePersistent,
            { static_cast<T>(0), static_cast<T>(entryCount > 0 ? entryCount - 1 : 0), static_cast<T>(defaultValue), static_cast<T>(1) },
            entries, entryCount, stableTextID, nullptr, groupName, 0);
    }

    void applyDefaultParameters() noexcept
    {
        core_.setInputGainDb(static_cast<T>(0));
        core_.setOutputGainDb(static_cast<T>(0));
        core_.setDryWetMix(static_cast<T>(1));
        core_.setQualityMode(PedalQualityMode::Normal);
        core_.setOversamplingMode(PedalOversamplingMode::x4);
        core_.preFilter().setHighPassFrequency(static_cast<T>(150));
        core_.preFilter().setHighPassSlopeIndex(0);
        core_.preFilter().setLowPassFrequency(static_cast<T>(16000));
        core_.preFilter().setMidGainDb(static_cast<T>(0));
        core_.clipper().setClipMode(PedalClipMode::Hard);
        core_.clipper().setDriveDb(static_cast<T>(34));
        core_.clipper().setPositiveThreshold(static_cast<T>(0.6));
        core_.clipper().setNegativeThreshold(static_cast<T>(0.6));
        core_.clipper().setThresholdLink(true);
        core_.clipper().setKnee(static_cast<T>(0));
        core_.clipper().setBias(static_cast<T>(0));
        core_.clipper().setAsymmetry(static_cast<T>(0));
        core_.clipper().setClipBlend(static_cast<T>(0));
        core_.postFilter().setHighPassFrequency(static_cast<T>(45));
        core_.postFilter().setLowPassSlopeIndex(0);
        core_.postFilter().setLowPassFrequency(static_cast<T>(6000));
        setScoopFrequencyHz(static_cast<T>(400));
        setScoopQ(static_cast<T>(1.2));
        setScoopAmount(static_cast<T>(0.5));
        setFizzCutHz(static_cast<T>(7000));
        setTone(static_cast<T>(0.55));
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T scoopAmount_ { static_cast<T>(0.5) };
    T scoopFrequencyHz_ { static_cast<T>(400) };
    T scoopQ_ { static_cast<T>(1.2) };
    T highBite_ { static_cast<T>(0.55) };
    PedalDriveCore<T> core_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_VINTAGEHARDDISTORTIONDSP_HPP
