#ifndef CVDSP_GUITAR_PEDALS_PHASERDSP_HPP
#define CVDSP_GUITAR_PEDALS_PHASERDSP_HPP

/**
 * @file PhaserDSP.hpp
 * @brief Guitar pedal wrapper around the generic CV_DSP phaser effect.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

#include "PedalGainStage.hpp"
#include "PedalParameterIDs.hpp"
#include "PedalParameterUtils.hpp"
#include "../../Core/AudioBufferView.hpp"
#include "../../Effects/Phaser.hpp"
#include "../../Manager/ParameterDescriptor.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief Four-stage classic guitar phaser with stable pedal descriptors.
 */
template<typename T = float>
class PhaserDSP
{
    static_assert(std::is_floating_point_v<T>, "PhaserDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 11>;

    constexpr PhaserDSP() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        inputGain_.prepare(sampleRate_);
        outputGain_.prepare(sampleRate_);
        phaser_.prepare(sampleRate_);
        applyDefaultParameters();
        reset();
    }

    void reset() noexcept
    {
        inputGain_.reset();
        outputGain_.reset();
        phaser_.reset();
    }

    void setBypassed(bool enabled) noexcept { bypassed_ = enabled; }
    void setInputGainDb(T db) noexcept { inputGain_.setGainDb(db); }
    void setLevelDb(T db) noexcept { outputGain_.setGainDb(db); }
    void setRateHz(T rateHz) noexcept { phaser_.setRate(rateHz); }
    void setDepth(T normalized) noexcept { phaser_.setDepth(normalized); }
    void setFeedback(T feedback) noexcept { phaser_.setFeedback(feedback); }
    void setMix(T normalized) noexcept { phaser_.setMix(normalized); }
    void setFrequencyRangeHz(T minHz, T maxHz) noexcept { phaser_.setFrequencyRange(minHz, maxHz); }
    void setStages(std::size_t stages) noexcept { phaser_.setStages(stages); }
    void setWaveform(LFOWaveform waveform) noexcept { phaser_.setWaveform(waveform); }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        if (bypassed_)
            return input;

        T output = inputGain_.processSample(input);
        output = phaser_.process(output);
        return outputGain_.processSample(output);
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

    [[nodiscard]] Phaser<T>& phaser() noexcept { return phaser_; }
    [[nodiscard]] const Phaser<T>& phaser() const noexcept { return phaser_; }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassed_; }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "phaser.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "phaser.input_gain", "Global"),
            makeFrequency(PedalParameterIDs::ModulationRate, "Rate", "Modulation Rate", 0.01, 20.0, 0.35, "phaser.rate", "Modulation"),
            makePercent(PedalParameterIDs::ModulationDepth, "Depth", "Modulation Depth", 0.85, "phaser.depth", "Modulation"),
            makeLinear(PedalParameterIDs::Feedback, "Feedback", "Feedback", -0.95, 0.95, 0.25, "phaser.feedback", "Voice"),
            makeLinear(PedalParameterIDs::StageCount, "Stages", "Stage Count", 2.0, 8.0, 4.0, "phaser.stages", "Voice", 1.0),
            makeFrequency(PedalParameterIDs::MinFrequency, "Min Freq", "Minimum Sweep Frequency", 80.0, 1200.0, 250.0, "phaser.min_frequency", "Sweep"),
            makeFrequency(PedalParameterIDs::MaxFrequency, "Max Freq", "Maximum Sweep Frequency", 600.0, 4000.0, 1600.0, "phaser.max_frequency", "Sweep"),
            makeEnum(PedalParameterIDs::LfoWaveform, "Wave", "LFO Waveform", kWaveformEntries.data(), kWaveformEntries.size(), 0.0, "phaser.waveform", "Modulation"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, 0.0, "phaser.level", "Global"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "phaser.mix", "Global")
        }};
        return descriptors;
    }

private:
    static constexpr manager::ParameterFlags kAutomatablePersistent =
        manager::toMask(manager::ParameterFlag::Automatable) |
        manager::toMask(manager::ParameterFlag::Persistent);

    static constexpr std::array<manager::ParameterEnumEntry, 4> kWaveformEntries {{
        { 0u, "Sine" }, { 1u, "Triangle" }, { 2u, "Saw" }, { 3u, "Square" }
    }};

    void applyDefaultParameters() noexcept
    {
        setBypassed(false);
        setInputGainDb(static_cast<T>(0));
        setRateHz(static_cast<T>(0.35));
        setDepth(static_cast<T>(0.85));
        setFeedback(static_cast<T>(0.25));
        setFrequencyRangeHz(static_cast<T>(250), static_cast<T>(1600));
        setStages(4);
        setWaveform(LFOWaveform::Sine);
        setLevelDb(static_cast<T>(0));
        setMix(static_cast<T>(1));
    }

    static Descriptor makeLinear(
        manager::ParameterID id,
        const char* shortName,
        const char* longName,
        double minimum,
        double maximum,
        double defaultValue,
        const char* stableTextID,
        const char* groupName,
        double step = 0.0) noexcept
    {
        return Descriptor(
            id,
            shortName,
            longName,
            manager::ParameterUnit::None,
            manager::ParameterScale::Linear,
            kAutomatablePersistent,
            { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue), static_cast<T>(step) },
            nullptr,
            0,
            stableTextID,
            nullptr,
            groupName,
            step > 0.0 ? 0u : 2u);
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
            2);
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

    PedalGainStage<T> inputGain_ {};
    PedalGainStage<T> outputGain_ {};
    Phaser<T> phaser_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PHASERDSP_HPP
