#ifndef CVDSP_GUITAR_PEDALS_CLASSICOVERDRIVEDSP_HPP
#define CVDSP_GUITAR_PEDALS_CLASSICOVERDRIVEDSP_HPP

/**
 * @file ClassicOverdriveDSP.hpp
 * @brief Mid-forward soft-clipping overdrive built on the common pedal core.
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
 * @brief Classic green-style overdrive with tight pre-filtering and soft clipping.
 *
 * The topology is intentionally compact and delegates the reusable processing to
 * PedalDriveCore: input gain, high-passed/mid-forward voice, cubic soft clipping,
 * DC blocking, simple tone low-pass/presence shaping, output level and dry/wet.
 */
template<typename T = float>
class ClassicOverdriveDSP
{
    static_assert(std::is_floating_point_v<T>, "ClassicOverdriveDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 14>;

    constexpr ClassicOverdriveDSP() noexcept = default;

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
    void setDrive(T normalized) noexcept { core_.setDriveDb(normalizedToDecibels(static_cast<T>(0), static_cast<T>(42), normalized)); }
    void setTone(T normalized) noexcept
    {
        const T tone = clamp01(normalized);
        core_.postFilter().setTone(tone);
        core_.postFilter().setLowPassFrequency(normalizedToLogFrequency(static_cast<T>(1000), static_cast<T>(4000), tone));
        core_.postFilter().setPresenceGainDb(normalizedToLinear(static_cast<T>(-2), static_cast<T>(4), tone));
    }
    void setLevelDb(T db) noexcept { core_.setOutputGainDb(db); }
    void setInputGainDb(T db) noexcept { core_.setInputGainDb(db); }
    void setMix(T normalized) noexcept { core_.setDryWetMix(normalized); }
    void setOversamplingMode(PedalOversamplingMode mode) noexcept { core_.setOversamplingMode(mode); }
    void setQualityMode(PedalQualityMode mode) noexcept { core_.setQualityMode(mode); }

    void setPreHighPassHz(T hz) noexcept { core_.preFilter().setHighPassFrequency(hz); }
    void setMidHumpGainDb(T db) noexcept { core_.preFilter().setMidGainDb(std::clamp(db, static_cast<T>(0), static_cast<T>(12))); }
    void setMidHumpFrequencyHz(T hz) noexcept { core_.preFilter().setMidFrequency(hz); }
    void setSoftness(T normalized) noexcept { core_.clipper().setKnee(normalized); }
    void setBias(T bipolar) noexcept { core_.clipper().setBias(bipolar); }
    void setAsymmetry(T normalized) noexcept { core_.clipper().setAsymmetry(normalized); }

    [[nodiscard]] PedalDriveCore<T>& core() noexcept { return core_; }
    [[nodiscard]] const PedalDriveCore<T>& core() const noexcept { return core_; }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "classic_overdrive.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "classic_overdrive.input_gain", "Global"),
            makePercent(PedalParameterIDs::Drive, "Drive", "Drive", 0.0, "classic_overdrive.drive", "Drive"),
            makePercent(PedalParameterIDs::Tone, "Tone", "Tone", 0.55, "classic_overdrive.tone", "Tone"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, 0.0, "classic_overdrive.level", "Global"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "classic_overdrive.mix", "Global"),
            makeEnum(PedalParameterIDs::Oversampling, "OS", "Oversampling", kOversamplingEntries.data(), kOversamplingEntries.size(), 2.0, "classic_overdrive.oversampling", "Advanced"),
            makeEnum(PedalParameterIDs::QualityMode, "Quality", "Quality Mode", kQualityEntries.data(), kQualityEntries.size(), 1.0, "classic_overdrive.quality", "Advanced"),
            makeFrequency(PedalParameterIDs::PreHighPassFrequency, "Pre HPF", "Pre High-Pass", 100.0, 1200.0, 720.0, "classic_overdrive.pre_hpf", "Voice"),
            makeDecibel(PedalParameterIDs::PreMidGain, "Hump", "Mid Hump Gain", 0.0, 12.0, 6.0, "classic_overdrive.mid_hump_gain", "Voice"),
            makeFrequency(PedalParameterIDs::PreMidFrequency, "Hump Freq", "Mid Hump Frequency", 400.0, 1500.0, 850.0, "classic_overdrive.mid_hump_freq", "Voice"),
            makePercent(PedalParameterIDs::Knee, "Soft", "Softness", 0.85, "classic_overdrive.softness", "Clip"),
            makeLinear(PedalParameterIDs::Bias, "Bias", "Bias", -1.0, 1.0, 0.0, "classic_overdrive.bias", "Clip"),
            makePercent(PedalParameterIDs::Asymmetry, "Asym", "Asymmetry", 0.0, "classic_overdrive.asymmetry", "Clip")
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

    static Descriptor makeBoolean(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        bool defaultValue,
        const char* stableTextID,
        manager::ParameterFlags extraFlags = 0u) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::None,
            manager::ParameterScale::Boolean,
            kAutomatablePersistent | extraFlags,
            { static_cast<T>(0), static_cast<T>(1), defaultValue ? static_cast<T>(1) : static_cast<T>(0), static_cast<T>(1) },
            nullptr,
            0,
            stableTextID,
            nullptr,
            "Global",
            0);
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
            { static_cast<T>(0), static_cast<T>(entryCount > 0 ? entryCount - 1 : 0), static_cast<T>(defaultValue), static_cast<T>(1) },
            entries,
            entryCount,
            stableTextID,
            nullptr,
            groupName,
            0);
    }

    void applyDefaultParameters() noexcept
    {
        core_.setInputGainDb(static_cast<T>(0));
        core_.setOutputGainDb(static_cast<T>(0));
        core_.setDryWetMix(static_cast<T>(1));
        core_.setQualityMode(PedalQualityMode::Normal);
        core_.setOversamplingMode(PedalOversamplingMode::x4);
        core_.preFilter().setHighPassFrequency(static_cast<T>(720));
        core_.preFilter().setHighPassSlopeIndex(1);
        core_.preFilter().setLowPassFrequency(static_cast<T>(18000));
        core_.preFilter().setMidFrequency(static_cast<T>(850));
        core_.preFilter().setMidGainDb(static_cast<T>(6));
        core_.preFilter().setMidQ(static_cast<T>(0.9));
        core_.clipper().setClipMode(PedalClipMode::Cubic);
        core_.clipper().setDriveDb(static_cast<T>(18));
        core_.clipper().setPositiveThreshold(static_cast<T>(1));
        core_.clipper().setNegativeThreshold(static_cast<T>(1));
        core_.clipper().setThresholdLink(true);
        core_.clipper().setKnee(static_cast<T>(0.85));
        core_.clipper().setBias(static_cast<T>(0));
        core_.clipper().setAsymmetry(static_cast<T>(0));
        core_.clipper().setClipBlend(static_cast<T>(0));
        core_.postFilter().setHighPassFrequency(static_cast<T>(40));
        core_.postFilter().setLowPassSlopeIndex(0);
        core_.postFilter().setMidFrequency(static_cast<T>(900));
        core_.postFilter().setMidQ(static_cast<T>(1));
        core_.postFilter().setMiddleGainDb(static_cast<T>(1.5));
        setTone(static_cast<T>(0.55));
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    PedalDriveCore<T> core_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_CLASSICOVERDRIVEDSP_HPP
