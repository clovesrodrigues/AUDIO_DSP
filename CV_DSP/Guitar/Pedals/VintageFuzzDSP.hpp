#ifndef CVDSP_GUITAR_PEDALS_VINTAGEFUZZDSP_HPP
#define CVDSP_GUITAR_PEDALS_VINTAGEFUZZDSP_HPP

/**
 * @file VintageFuzzDSP.hpp
 * @brief Biasable vintage fuzz with cleanup, input loading and starve controls.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "PedalDriveCore.hpp"
#include "PedalParameterIDs.hpp"
#include "../../Dynamics/NoiseGate.hpp"
#include "../../Filters/DCBlocker.hpp"
#include "../../Manager/ParameterDescriptor.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief Vintage asymmetric fuzz inspired by germanium/silicon transistor pedals.
 *
 * The implementation keeps the common PedalDriveCore signal path but configures
 * it for loose low-end, aggressive asymmetry, foldback texture, optional output
 * rectification, post-rectifier DC blocking, and a lightweight input-load /
 * cleanup / starve control surface.
 */
template<typename T = float>
class VintageFuzzDSP
{
    static_assert(std::is_floating_point_v<T>, "VintageFuzzDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 22>;

    constexpr VintageFuzzDSP() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        core_.prepare(sampleRate_);
        outputDcBlocker_.prepare(sampleRate_);
        outputDcBlocker_.setCutoffHz(static_cast<T>(8));
        gate_.prepare(sampleRate_);
        applyDefaultParameters();
        reset();
    }

    void reset() noexcept
    {
        core_.reset();
        outputDcBlocker_.reset();
        gate_.reset();
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        const T gated = gateEnabled_ ? gate_.process(input) : input;
        T output = core_.processSample(gated);
        output = applyRectify(output);
        return outputDcBlocker_.process(output);
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

    void setBypassed(bool enabled) noexcept { core_.setBypassed(enabled); }
    void setFuzz(T normalized) noexcept { fuzz_ = clamp01(normalized); updateDriveModel(); }
    void setLevelDb(T db) noexcept { core_.setOutputGainDb(db); }
    void setTone(T normalized) noexcept
    {
        tone_ = clamp01(normalized);
        core_.postFilter().setLowPassFrequency(normalizedToLogFrequency(static_cast<T>(1200), static_cast<T>(8000), tone_));
        core_.postFilter().setPresenceGainDb(normalizedToLinear(static_cast<T>(-5), static_cast<T>(3), tone_));
    }
    void setInputGainDb(T db) noexcept { core_.setInputGainDb(db); }
    void setMix(T normalized) noexcept { core_.setDryWetMix(normalized); }
    void setOversamplingMode(PedalOversamplingMode mode) noexcept { core_.setOversamplingMode(mode); }
    void setQualityMode(PedalQualityMode mode) noexcept { core_.setQualityMode(mode); }

    void setBias(T bipolar) noexcept
    {
        bias_ = std::clamp(bipolar, static_cast<T>(-1), static_cast<T>(1));
        core_.clipper().setBias(bias_ * static_cast<T>(0.45));
    }

    void setStarve(T normalized) noexcept
    {
        starve_ = clamp01(normalized);
        updateDriveModel();
    }

    void setInputLoad(T normalized) noexcept
    {
        inputLoad_ = clamp01(normalized);
        core_.preFilter().setLowPassFrequency(normalizedToLogFrequency(static_cast<T>(2500), static_cast<T>(18000), static_cast<T>(1) - inputLoad_));
        core_.preFilter().setTrebleGainDb(normalizedToLinear(static_cast<T>(0), static_cast<T>(-8), inputLoad_));
    }

    void setCleanup(T normalized) noexcept
    {
        cleanup_ = clamp01(normalized);
        updateDriveModel();
    }

    void setAsymmetry(T normalized) noexcept { core_.clipper().setAsymmetry(normalized); }
    void setPositiveGain(T gain) noexcept { positiveGain_ = std::clamp(gain, static_cast<T>(0.1), static_cast<T>(5)); updateDriveModel(); }
    void setNegativeGain(T gain) noexcept { negativeGain_ = std::clamp(gain, static_cast<T>(0.05), static_cast<T>(2)); updateDriveModel(); }
    void setFoldbackAmount(T normalized) noexcept { core_.clipper().setFoldbackAmount(normalized); }
    void setRectifyMode(PedalRectifyMode mode) noexcept { rectifyMode_ = mode; }
    void setPostLowPassHz(T hz) noexcept { core_.postFilter().setLowPassFrequency(hz); }

    void setLowBloom(T normalized) noexcept
    {
        lowBloom_ = clamp01(normalized);
        core_.preFilter().setHighPassFrequency(normalizedToLogFrequency(static_cast<T>(30), static_cast<T>(120), static_cast<T>(1) - lowBloom_));
        core_.postFilter().setBassGainDb(normalizedToLinear(static_cast<T>(-2), static_cast<T>(5), lowBloom_));
    }

    void setGateEnabled(bool enabled) noexcept { gateEnabled_ = enabled; }
    void setGateThresholdDb(T db) noexcept
    {
        gateThresholdDb_ = std::clamp(db, static_cast<T>(-90), static_cast<T>(-20));
        gate_.setThresholdOpenDB(gateThresholdDb_);
        gate_.setThresholdCloseDB(gateThresholdDb_ - static_cast<T>(5));
    }

    [[nodiscard]] PedalDriveCore<T>& core() noexcept { return core_; }
    [[nodiscard]] const PedalDriveCore<T>& core() const noexcept { return core_; }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "vintage_fuzz.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "vintage_fuzz.input_gain", "Global"),
            makePercent(PedalParameterIDs::Drive, "Fuzz", "Fuzz", 0.82, "vintage_fuzz.fuzz", "Drive"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, -3.0, "vintage_fuzz.level", "Global"),
            makePercent(PedalParameterIDs::Tone, "Tone", "Tone", 0.42, "vintage_fuzz.tone", "Tone"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "vintage_fuzz.mix", "Global"),
            makeEnum(PedalParameterIDs::Oversampling, "OS", "Oversampling", kOversamplingEntries.data(), kOversamplingEntries.size(), 3.0, "vintage_fuzz.oversampling", "Advanced"),
            makeEnum(PedalParameterIDs::QualityMode, "Quality", "Quality Mode", kQualityEntries.data(), kQualityEntries.size(), 1.0, "vintage_fuzz.quality", "Advanced"),
            makeLinear(PedalParameterIDs::Bias, "Bias", "Bias", -1.0, 1.0, 0.12, "vintage_fuzz.bias", "Clip"),
            makePercent(PedalParameterIDs::InterstageGain, "Starve", "Starve", 0.18, "vintage_fuzz.starve", "Clip"),
            makePercent(PedalParameterIDs::ClipBlend, "Cleanup", "Cleanup", 0.35, "vintage_fuzz.cleanup", "Drive"),
            makePercent(PedalParameterIDs::PreLowPassFrequency, "Load", "Input Load", 0.45, "vintage_fuzz.input_load", "Voice"),
            makePercent(PedalParameterIDs::Asymmetry, "Asym", "Asymmetry", 0.75, "vintage_fuzz.asymmetry", "Clip"),
            makeLinear(PedalParameterIDs::PositiveThreshold, "+Gain", "Positive Gain", 0.1, 5.0, 1.0, "vintage_fuzz.positive_gain", "Clip"),
            makeLinear(PedalParameterIDs::NegativeThreshold, "-Gain", "Negative Gain", 0.05, 2.0, 0.35, "vintage_fuzz.negative_gain", "Clip"),
            makePercent(PedalParameterIDs::FoldbackAmount, "Fold", "Foldback Amount", 0.2, "vintage_fuzz.foldback", "Clip"),
            makeEnum(PedalParameterIDs::RectifyMode, "Rect", "Rectify Mode", kRectifyEntries.data(), kRectifyEntries.size(), 0.0, "vintage_fuzz.rectify", "Clip"),
            makeFrequency(PedalParameterIDs::PreHighPassFrequency, "Pre HPF", "Pre High-Pass", 20.0, 300.0, 40.0, "vintage_fuzz.pre_hpf", "Voice"),
            makeFrequency(PedalParameterIDs::PostLowPassFrequency, "Post LPF", "Post Low-Pass", 1000.0, 8000.0, 3500.0, "vintage_fuzz.post_lpf", "Tone"),
            makePercent(PedalParameterIDs::Bass, "Bloom", "Low Bloom", 0.65, "vintage_fuzz.low_bloom", "Tone"),
            makeBoolean(PedalParameterIDs::GateEnable, "Gate", "Gate Enable", false, "vintage_fuzz.gate_enable"),
            makeDecibel(PedalParameterIDs::GateThreshold, "Gate Th", "Gate Threshold", -90.0, -20.0, -70.0, "vintage_fuzz.gate_threshold", "Gate")
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

    static constexpr std::array<manager::ParameterEnumEntry, 3> kRectifyEntries {{
        { 0u, "Off" }, { 1u, "Half" }, { 2u, "Full" }
    }};

    static Descriptor makeLinear(manager::ParameterID id, const char* shortName, const char* longName,
        double minimum, double maximum, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::None, manager::ParameterScale::Linear,
            kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, nullptr, groupName, 2);
    }

    static Descriptor makePercent(manager::ParameterID id, const char* shortName, const char* longName,
        double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Percent, manager::ParameterScale::Percentage,
            kAutomatablePersistent, { static_cast<T>(0), static_cast<T>(1), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, "%", groupName, 1);
    }

    static Descriptor makeDecibel(manager::ParameterID id, const char* shortName, const char* longName,
        double minimum, double maximum, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Decibels, manager::ParameterScale::Decibel,
            kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, "dB", groupName, 1);
    }

    static Descriptor makeFrequency(manager::ParameterID id, const char* shortName, const char* longName,
        double minimum, double maximum, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Hertz, manager::ParameterScale::Logarithmic,
            kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) },
            nullptr, 0, stableTextID, "Hz", groupName, 1);
    }

    static Descriptor makeBoolean(manager::ParameterID id, const char* shortName, const char* longName,
        bool defaultValue, const char* stableTextID, manager::ParameterFlags extraFlags = 0u) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::None, manager::ParameterScale::Boolean,
            kAutomatablePersistent | extraFlags,
            { static_cast<T>(0), static_cast<T>(1), defaultValue ? static_cast<T>(1) : static_cast<T>(0), static_cast<T>(1) },
            nullptr, 0, stableTextID, nullptr, "Global", 0);
    }

    static Descriptor makeEnum(manager::ParameterID id, const char* shortName, const char* longName,
        const manager::ParameterEnumEntry* entries, std::size_t entryCount, double defaultValue,
        const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Index, manager::ParameterScale::Enum,
            kAutomatablePersistent,
            { static_cast<T>(0), static_cast<T>(entryCount > 0 ? entryCount - 1 : 0), static_cast<T>(defaultValue), static_cast<T>(1) },
            entries, entryCount, stableTextID, nullptr, groupName, 0);
    }

    [[nodiscard]] T applyRectify(T input) noexcept
    {
        switch (rectifyMode_)
        {
            case PedalRectifyMode::Half:
                return input > static_cast<T>(0) ? input : static_cast<T>(0);
            case PedalRectifyMode::Full:
                return std::abs(input);
            case PedalRectifyMode::Off:
            default:
                return input;
        }
    }

    void updateDriveModel() noexcept
    {
        const T cleanupReduction = normalizedToLinear(static_cast<T>(1), static_cast<T>(0.55), cleanup_);
        const T starveBoost = normalizedToLinear(static_cast<T>(0), static_cast<T>(10), starve_);
        core_.setDriveDb((normalizedToDecibels(static_cast<T>(18), static_cast<T>(60), fuzz_) + starveBoost) * cleanupReduction);

        const T headroom = normalizedToLinear(static_cast<T>(1), static_cast<T>(0.22), starve_);
        core_.clipper().setPositiveThreshold(std::clamp(headroom / positiveGain_, PedalConstants<T>::kMinClipThreshold, PedalConstants<T>::kMaxClipThreshold));
        core_.clipper().setNegativeThreshold(std::clamp((headroom * static_cast<T>(1.65)) / negativeGain_, PedalConstants<T>::kMinClipThreshold, PedalConstants<T>::kMaxClipThreshold));
        core_.clipper().setClipBlend(normalizedToLinear(static_cast<T>(0.15), static_cast<T>(0.55), starve_));
    }

    void applyDefaultParameters() noexcept
    {
        core_.setInputGainDb(static_cast<T>(0));
        core_.setOutputGainDb(static_cast<T>(-3));
        core_.setDryWetMix(static_cast<T>(1));
        core_.setQualityMode(PedalQualityMode::Normal);
        core_.setOversamplingMode(PedalOversamplingMode::x8);
        core_.preFilter().setHighPassFrequency(static_cast<T>(40));
        core_.preFilter().setHighPassSlopeIndex(0);
        core_.preFilter().setMidFrequency(static_cast<T>(650));
        core_.preFilter().setMidGainDb(static_cast<T>(2));
        core_.clipper().setClipMode(PedalClipMode::Foldback);
        core_.clipper().setThresholdLink(false);
        core_.clipper().setKnee(static_cast<T>(0.45));
        setFuzz(static_cast<T>(0.82));
        setBias(static_cast<T>(0.12));
        setStarve(static_cast<T>(0.18));
        setInputLoad(static_cast<T>(0.45));
        setCleanup(static_cast<T>(0.35));
        setAsymmetry(static_cast<T>(0.75));
        setPositiveGain(static_cast<T>(1));
        setNegativeGain(static_cast<T>(0.35));
        setFoldbackAmount(static_cast<T>(0.2));
        setRectifyMode(PedalRectifyMode::Off);
        setLowBloom(static_cast<T>(0.65));
        setTone(static_cast<T>(0.42));
        setPostLowPassHz(static_cast<T>(3500));
        setGateEnabled(false);
        setGateThresholdDb(static_cast<T>(-70));
        gate_.setAttackMs(static_cast<T>(0.5));
        gate_.setHoldMs(static_cast<T>(35));
        gate_.setReleaseMs(static_cast<T>(60));
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T fuzz_ { static_cast<T>(0.82) };
    T tone_ { static_cast<T>(0.42) };
    T bias_ { static_cast<T>(0.12) };
    T starve_ { static_cast<T>(0.18) };
    T inputLoad_ { static_cast<T>(0.45) };
    T cleanup_ { static_cast<T>(0.35) };
    T positiveGain_ { static_cast<T>(1) };
    T negativeGain_ { static_cast<T>(0.35) };
    T lowBloom_ { static_cast<T>(0.65) };
    T gateThresholdDb_ { static_cast<T>(-70) };
    bool gateEnabled_ { false };
    PedalRectifyMode rectifyMode_ { PedalRectifyMode::Off };
    PedalDriveCore<T> core_ {};
    filters::DCBlocker<T> outputDcBlocker_ {};
    dynamics::NoiseGate<T> gate_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_VINTAGEFUZZDSP_HPP
