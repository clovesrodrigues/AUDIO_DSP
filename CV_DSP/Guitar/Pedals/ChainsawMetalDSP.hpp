#ifndef CVDSP_GUITAR_PEDALS_CHAINSAWMETALDSP_HPP
#define CVDSP_GUITAR_PEDALS_CHAINSAWMETALDSP_HPP

/**
 * @file ChainsawMetalDSP.hpp
 * @brief High-gain chainsaw distortion with dual-stage clipping and resonant EQ.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

#include "PedalClipper.hpp"
#include "PedalGainStage.hpp"
#include "PedalMix.hpp"
#include "PedalParameterIDs.hpp"
#include "PedalParameterUtils.hpp"
#include "../../Core/AudioBufferView.hpp"
#include "../../Dynamics/NoiseGate.hpp"
#include "../../Filters/Biquad.hpp"
#include "../../Filters/DCBlocker.hpp"
#include "../../Manager/ParameterDescriptor.hpp"
#include "../../Math/Oversampling.hpp"

namespace cvdsp::guitar::pedals
{

/**
 * @brief Swedish-chainsaw/high-gain pedal with multi-stage clipping.
 *
 * Signal flow: gate → input gain → tight low cut → pre boost → oversampled
 * stage-1 soft clipping → interstage gain → stage-2 hard clipping → DC block →
 * resonant low-mid/high-mid post EQ → fizz cut → output gain → dry/wet mix.
 */
template<typename T = float>
class ChainsawMetalDSP
{
    static_assert(std::is_floating_point_v<T>, "ChainsawMetalDSP requires a floating point type");

public:
    using value_type = T;
    using Descriptor = manager::ParameterDescriptor<T>;
    using DescriptorArray = std::array<Descriptor, 27>;

    constexpr ChainsawMetalDSP() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        inputGain_.prepare(sampleRate_);
        interstageGain_.prepare(sampleRate_);
        outputGain_.prepare(sampleRate_);
        mix_.prepare(sampleRate_);
        stage1_.prepare(sampleRate_ * static_cast<T>(8));
        stage2_.prepare(sampleRate_ * static_cast<T>(8));
        dcBlocker_.prepare(sampleRate_ * static_cast<T>(8));
        dcBlocker_.setCutoffHz(static_cast<T>(8));
        gate_.prepare(sampleRate_);
        prepareFilters();
        oversampling2x_.prepare(sampleRate_);
        oversampling4x_.prepare(sampleRate_);
        oversampling8x_.prepare(sampleRate_);
        applyDefaultParameters();
        reset();
    }

    void reset() noexcept
    {
        inputGain_.reset();
        interstageGain_.reset();
        outputGain_.reset();
        mix_.reset();
        stage1_.reset();
        stage2_.reset();
        dcBlocker_.reset();
        gate_.reset();
        resetFilters();
        oversampling2x_.reset();
        oversampling4x_.reset();
        oversampling8x_.reset();
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        if (bypassed_)
            return input;

        const T dry = input;
        T wet = gateEnabled_ ? gate_.process(input) : input;
        wet = inputGain_.processSample(wet);
        wet = tightLowCut_.process(wet);
        wet = preBoost_.process(wet);
        wet = processNonlinear(wet);
        wet = lowMid_.process(wet);
        wet = highMid_.process(wet);
        wet = fizzCut_.process(wet);
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

    void setBypassed(bool enabled) noexcept { bypassed_ = enabled; }
    void setInputGainDb(T db) noexcept { inputGain_.setGainDb(db); }
    void setGain(T normalized) noexcept { setStage2Drive(normalized); setStage1Drive(normalized * static_cast<T>(0.7)); }
    void setLevelDb(T db) noexcept { outputGain_.setGainDb(db); }
    void setMix(T normalized) noexcept { mix_.setMix(normalized); }
    void setOversamplingMode(PedalOversamplingMode mode) noexcept { oversamplingMode_ = mode; }
    void setQualityMode(PedalQualityMode mode) noexcept { qualityMode_ = mode; stage1_.setQualityMode(mode); stage2_.setQualityMode(mode); }
    void setVoiceMode(ChainsawVoiceMode mode) noexcept { voiceMode_ = mode; applyVoiceMode(mode); }

    void setPreBoostGainDb(T db) noexcept { preBoostGainDb_ = clampEqGain(db, static_cast<T>(0), static_cast<T>(18)); updatePreBoost(); }
    void setPreBoostFrequencyHz(T hz) noexcept { preBoostFrequencyHz_ = clampFrequency(hz); updatePreBoost(); }
    void setPreBoostQ(T q) noexcept { preBoostQ_ = clampQ(q); updatePreBoost(); }
    void setStage1Drive(T normalized) noexcept { stage1Drive_ = clamp01(normalized); stage1_.setDriveDb(normalizedToDecibels(static_cast<T>(6), static_cast<T>(42), stage1Drive_)); }
    void setStage1Softness(T normalized) noexcept { stage1_.setKnee(normalized); }
    void setStage2Drive(T normalized) noexcept { stage2Drive_ = clamp01(normalized); stage2_.setDriveDb(normalizedToDecibels(static_cast<T>(12), static_cast<T>(60), stage2Drive_)); }
    void setHardThreshold(T threshold) noexcept { hardThreshold_ = std::clamp(threshold, static_cast<T>(0.05), static_cast<T>(1)); stage2_.setPositiveThreshold(hardThreshold_); stage2_.setNegativeThreshold(hardThreshold_); }
    void setInterstageGainDb(T db) noexcept { interstageGain_.setGainDb(std::clamp(db, static_cast<T>(-12), static_cast<T>(36))); }
    void setLowMidGainDb(T db) noexcept { lowMidGainDb_ = clampEqGain(db, static_cast<T>(-12), static_cast<T>(18)); updateLowMid(); }
    void setLowMidFrequencyHz(T hz) noexcept { lowMidFrequencyHz_ = clampFrequency(hz); updateLowMid(); }
    void setLowMidQ(T q) noexcept { lowMidQ_ = clampQ(q); updateLowMid(); }
    void setHighMidGainDb(T db) noexcept { highMidGainDb_ = clampEqGain(db, static_cast<T>(-12), static_cast<T>(24)); updateHighMid(); }
    void setHighMidFrequencyHz(T hz) noexcept { highMidFrequencyHz_ = clampFrequency(hz); updateHighMid(); }
    void setHighMidQ(T q) noexcept { highMidQ_ = clampQ(q); updateHighMid(); }
    void setFizzCutHz(T hz) noexcept { fizzCutHz_ = clampFrequency(hz); updateFizzCut(); }
    void setTightLowCutHz(T hz) noexcept { tightLowCutHz_ = clampFrequency(hz); updateTightLowCut(); }
    void setGateEnabled(bool enabled) noexcept { gateEnabled_ = enabled; }
    void setGateThresholdDb(T db) noexcept { gateThresholdDb_ = std::clamp(db, static_cast<T>(-90), static_cast<T>(-20)); gate_.setThresholdOpenDB(gateThresholdDb_); gate_.setThresholdCloseDB(gateThresholdDb_ - static_cast<T>(5)); }
    void setGateReleaseMs(T ms) noexcept { gate_.setReleaseMs(std::clamp(ms, static_cast<T>(5), static_cast<T>(500))); }

    [[nodiscard]] static const DescriptorArray& getParameterDescriptors() noexcept
    {
        static const DescriptorArray descriptors {{
            makeBoolean(PedalParameterIDs::Bypass, "Bypass", "Bypass", false, "chainsaw_metal.bypass", manager::toMask(manager::ParameterFlag::Bypass)),
            makeDecibel(PedalParameterIDs::InputGain, "Input", "Input Gain", -24.0, 24.0, 0.0, "chainsaw_metal.input_gain", "Global"),
            makePercent(PedalParameterIDs::Drive, "Gain", "Gain", 0.9, "chainsaw_metal.gain", "Drive"),
            makeDecibel(PedalParameterIDs::OutputLevel, "Level", "Output Level", -36.0, 12.0, -6.0, "chainsaw_metal.level", "Global"),
            makePercent(PedalParameterIDs::DryWetMix, "Mix", "Dry/Wet Mix", 1.0, "chainsaw_metal.mix", "Global"),
            makeEnum(PedalParameterIDs::Oversampling, "OS", "Oversampling", kOversamplingEntries.data(), kOversamplingEntries.size(), 3.0, "chainsaw_metal.oversampling", "Advanced"),
            makeEnum(PedalParameterIDs::QualityMode, "Quality", "Quality Mode", kQualityEntries.data(), kQualityEntries.size(), 1.0, "chainsaw_metal.quality", "Advanced"),
            makeEnum(PedalParameterIDs::VoiceMode, "Voice", "Voice Mode", kVoiceEntries.data(), kVoiceEntries.size(), 0.0, "chainsaw_metal.voice", "Voice"),
            makeDecibel(PedalParameterIDs::PreBoostGain, "Pre Boost", "Pre Boost Gain", 0.0, 18.0, 6.0, "chainsaw_metal.pre_boost_gain", "Voice"),
            makeFrequency(PedalParameterIDs::PreBoostFrequency, "Pre Freq", "Pre Boost Frequency", 500.0, 2500.0, 1000.0, "chainsaw_metal.pre_boost_freq", "Voice"),
            makeLinear(PedalParameterIDs::PreBoostQ, "Pre Q", "Pre Boost Q", 0.5, 8.0, 2.0, "chainsaw_metal.pre_boost_q", "Voice"),
            makePercent(PedalParameterIDs::Stage1Drive, "S1 Drive", "Stage 1 Drive", 0.55, "chainsaw_metal.stage1_drive", "Drive"),
            makePercent(PedalParameterIDs::Stage1Softness, "S1 Soft", "Stage 1 Softness", 0.65, "chainsaw_metal.stage1_softness", "Drive"),
            makePercent(PedalParameterIDs::Stage2Drive, "S2 Drive", "Stage 2 Drive", 0.9, "chainsaw_metal.stage2_drive", "Drive"),
            makeLinear(PedalParameterIDs::HardThreshold, "Thresh", "Hard Threshold", 0.05, 1.0, 0.3, "chainsaw_metal.hard_threshold", "Drive"),
            makeDecibel(PedalParameterIDs::InterstageGain, "Inter", "Interstage Gain", -12.0, 36.0, 12.0, "chainsaw_metal.interstage_gain", "Drive"),
            makeDecibel(PedalParameterIDs::LowMidGain, "LM Gain", "Low-Mid Gain", -12.0, 18.0, 12.0, "chainsaw_metal.low_mid_gain", "Tone"),
            makeFrequency(PedalParameterIDs::LowMidFrequency, "LM Freq", "Low-Mid Frequency", 200.0, 800.0, 400.0, "chainsaw_metal.low_mid_freq", "Tone"),
            makeLinear(PedalParameterIDs::LowMidQ, "LM Q", "Low-Mid Q", 0.5, 5.0, 1.4, "chainsaw_metal.low_mid_q", "Tone"),
            makeDecibel(PedalParameterIDs::HighMidGain, "HM Gain", "High-Mid Gain", -12.0, 24.0, 15.0, "chainsaw_metal.high_mid_gain", "Tone"),
            makeFrequency(PedalParameterIDs::HighMidFrequency, "HM Freq", "High-Mid Frequency", 800.0, 2500.0, 1200.0, "chainsaw_metal.high_mid_freq", "Tone"),
            makeLinear(PedalParameterIDs::HighMidQ, "HM Q", "High-Mid Q", 0.5, 8.0, 2.5, "chainsaw_metal.high_mid_q", "Tone"),
            makeFrequency(PedalParameterIDs::FizzCutFrequency, "Fizz", "Fizz Cut", 3000.0, 12000.0, 6000.0, "chainsaw_metal.fizz_cut", "Tone"),
            makeFrequency(PedalParameterIDs::TightLowCut, "Low Cut", "Tight Low Cut", 40.0, 250.0, 80.0, "chainsaw_metal.tight_low_cut", "Voice"),
            makeBoolean(PedalParameterIDs::GateEnable, "Gate", "Gate Enable", true, "chainsaw_metal.gate_enable"),
            makeDecibel(PedalParameterIDs::GateThreshold, "Gate Th", "Gate Threshold", -90.0, -20.0, -60.0, "chainsaw_metal.gate_threshold", "Gate"),
            makeLinear(PedalParameterIDs::GateRelease, "Gate Rel", "Gate Release", 5.0, 500.0, 80.0, "chainsaw_metal.gate_release", "Gate")
        }};
        return descriptors;
    }

private:
    static constexpr manager::ParameterFlags kAutomatablePersistent = manager::toMask(manager::ParameterFlag::Automatable) | manager::toMask(manager::ParameterFlag::Persistent);
    static constexpr std::array<manager::ParameterEnumEntry, 4> kOversamplingEntries {{ { 0u, "Off" }, { 1u, "2x" }, { 2u, "4x" }, { 3u, "8x" } }};
    static constexpr std::array<manager::ParameterEnumEntry, 3> kQualityEntries {{ { 0u, "Eco" }, { 1u, "Normal" }, { 2u, "Studio" } }};
    static constexpr std::array<manager::ParameterEnumEntry, 4> kVoiceEntries {{ { 0u, "Classic Swedish" }, { 1u, "Modern Tight" }, { 2u, "Doom Loose" }, { 3u, "Death Metal Scoop" } }};

    static Descriptor makeLinear(manager::ParameterID id, const char* shortName, const char* longName, double minimum, double maximum, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::None, manager::ParameterScale::Linear, kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) }, nullptr, 0, stableTextID, nullptr, groupName, 2);
    }
    static Descriptor makePercent(manager::ParameterID id, const char* shortName, const char* longName, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Percent, manager::ParameterScale::Percentage, kAutomatablePersistent, { static_cast<T>(0), static_cast<T>(1), static_cast<T>(defaultValue) }, nullptr, 0, stableTextID, "%", groupName, 1);
    }
    static Descriptor makeDecibel(manager::ParameterID id, const char* shortName, const char* longName, double minimum, double maximum, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Decibels, manager::ParameterScale::Decibel, kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) }, nullptr, 0, stableTextID, "dB", groupName, 1);
    }
    static Descriptor makeFrequency(manager::ParameterID id, const char* shortName, const char* longName, double minimum, double maximum, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Hertz, manager::ParameterScale::Logarithmic, kAutomatablePersistent, { static_cast<T>(minimum), static_cast<T>(maximum), static_cast<T>(defaultValue) }, nullptr, 0, stableTextID, "Hz", groupName, 1);
    }
    static Descriptor makeBoolean(manager::ParameterID id, const char* shortName, const char* longName, bool defaultValue, const char* stableTextID, manager::ParameterFlags extraFlags = 0u) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::None, manager::ParameterScale::Boolean, kAutomatablePersistent | extraFlags, { static_cast<T>(0), static_cast<T>(1), defaultValue ? static_cast<T>(1) : static_cast<T>(0), static_cast<T>(1) }, nullptr, 0, stableTextID, nullptr, "Global", 0);
    }
    static Descriptor makeEnum(manager::ParameterID id, const char* shortName, const char* longName, const manager::ParameterEnumEntry* entries, std::size_t entryCount, double defaultValue, const char* stableTextID, const char* groupName) noexcept
    {
        return Descriptor(id, shortName, longName, manager::ParameterUnit::Index, manager::ParameterScale::Enum, kAutomatablePersistent, { static_cast<T>(0), static_cast<T>(entryCount > 0 ? entryCount - 1 : 0), static_cast<T>(defaultValue), static_cast<T>(1) }, entries, entryCount, stableTextID, nullptr, groupName, 0);
    }

    void prepareFilters() noexcept
    {
        tightLowCut_.prepare(sampleRate_); preBoost_.prepare(sampleRate_); lowMid_.prepare(sampleRate_); highMid_.prepare(sampleRate_); fizzCut_.prepare(sampleRate_);
    }
    void resetFilters() noexcept
    {
        tightLowCut_.reset(); preBoost_.reset(); lowMid_.reset(); highMid_.reset(); fizzCut_.reset();
    }
    [[nodiscard]] static constexpr T clampEqGain(T db, T minimum, T maximum) noexcept { return db < minimum ? minimum : (db > maximum ? maximum : db); }
    [[nodiscard]] static constexpr T clampQ(T q) noexcept { return q < PedalConstants<T>::kMinQ ? PedalConstants<T>::kMinQ : (q > PedalConstants<T>::kMaxQ ? PedalConstants<T>::kMaxQ : q); }
    [[nodiscard]] T clampFrequency(T hz) const noexcept { return std::clamp(hz, PedalConstants<T>::kMinFrequencyHz, std::min(PedalConstants<T>::kMaxFrequencyHz, sampleRate_ * static_cast<T>(0.49))); }
    void configure(filters::Biquad<T>& filter, filters::BiquadType type, T frequency, T q, T gainDb = static_cast<T>(0)) noexcept
    {
        filter.setType(type); filter.setFrequency(clampFrequency(frequency)); filter.setQ(clampQ(q)); filter.setGainDB(gainDb); filter.updateCoefficients();
    }
    void updateTightLowCut() noexcept { configure(tightLowCut_, filters::BiquadType::HighPass, tightLowCutHz_, PedalConstants<T>::kDefaultQ); }
    void updatePreBoost() noexcept { configure(preBoost_, filters::BiquadType::PeakingEQ, preBoostFrequencyHz_, preBoostQ_, preBoostGainDb_); }
    void updateLowMid() noexcept { configure(lowMid_, filters::BiquadType::PeakingEQ, lowMidFrequencyHz_, lowMidQ_, lowMidGainDb_); }
    void updateHighMid() noexcept { configure(highMid_, filters::BiquadType::PeakingEQ, highMidFrequencyHz_, highMidQ_, highMidGainDb_); }
    void updateFizzCut() noexcept { configure(fizzCut_, filters::BiquadType::LowPass, fizzCutHz_, static_cast<T>(0.55)); }
    void updateAllFilters() noexcept { updateTightLowCut(); updatePreBoost(); updateLowMid(); updateHighMid(); updateFizzCut(); }

    [[nodiscard]] inline T processNonlinear(T input) noexcept
    {
        switch (oversamplingMode_)
        {
            case PedalOversamplingMode::x2: return processOversampled(oversampling2x_, input);
            case PedalOversamplingMode::x4: return processOversampled(oversampling4x_, input);
            case PedalOversamplingMode::x8: return processOversampled(oversampling8x_, input);
            case PedalOversamplingMode::Off:
            default:
                return processClipChain(input);
        }
    }
    template<std::size_t Factor>
    [[nodiscard]] inline T processOversampled(Oversampling<T, Factor>& oversampler, T input) noexcept
    {
        auto block = oversampler.processUp(input);
        for (std::size_t i = 0; i < Factor; ++i)
            block[i] = processClipChain(block[i]);
        return oversampler.processDown(block);
    }
    [[nodiscard]] inline T processClipChain(T input) noexcept
    {
        T y = stage1_.processSample(input);
        y = interstageGain_.processSample(y);
        y = stage2_.processSample(y);
        return dcBlocker_.process(y);
    }

    void applyVoiceMode(ChainsawVoiceMode mode) noexcept
    {
        switch (mode)
        {
            case ChainsawVoiceMode::ModernTight:
                setTightLowCutHz(static_cast<T>(120)); setFizzCutHz(static_cast<T>(5500)); setLowMidGainDb(static_cast<T>(9)); setHighMidGainDb(static_cast<T>(12)); setHardThreshold(static_cast<T>(0.26)); break;
            case ChainsawVoiceMode::DoomLoose:
                setTightLowCutHz(static_cast<T>(45)); setFizzCutHz(static_cast<T>(4200)); setLowMidGainDb(static_cast<T>(15)); setHighMidGainDb(static_cast<T>(10)); setHardThreshold(static_cast<T>(0.38)); break;
            case ChainsawVoiceMode::DeathMetalScoop:
                setTightLowCutHz(static_cast<T>(95)); setFizzCutHz(static_cast<T>(6800)); setLowMidGainDb(static_cast<T>(8)); setHighMidGainDb(static_cast<T>(18)); setHardThreshold(static_cast<T>(0.24)); break;
            case ChainsawVoiceMode::ClassicSwedish:
            default:
                setTightLowCutHz(static_cast<T>(80)); setFizzCutHz(static_cast<T>(6000)); setLowMidGainDb(static_cast<T>(12)); setHighMidGainDb(static_cast<T>(15)); setHardThreshold(static_cast<T>(0.3)); break;
        }
    }

    void applyDefaultParameters() noexcept
    {
        setInputGainDb(static_cast<T>(0)); setLevelDb(static_cast<T>(-6)); setMix(static_cast<T>(1)); setQualityMode(PedalQualityMode::Normal); setOversamplingMode(PedalOversamplingMode::x8);
        stage1_.setClipMode(PedalClipMode::Cubic); stage1_.setPositiveThreshold(static_cast<T>(1)); stage1_.setNegativeThreshold(static_cast<T>(1)); stage1_.setThresholdLink(true); setStage1Softness(static_cast<T>(0.65));
        stage2_.setClipMode(PedalClipMode::Hard); stage2_.setThresholdLink(true); stage2_.setKnee(static_cast<T>(0)); setStage1Drive(static_cast<T>(0.55)); setStage2Drive(static_cast<T>(0.9)); setInterstageGainDb(static_cast<T>(12));
        setPreBoostGainDb(static_cast<T>(6)); setPreBoostFrequencyHz(static_cast<T>(1000)); setPreBoostQ(static_cast<T>(2));
        setLowMidFrequencyHz(static_cast<T>(400)); setLowMidQ(static_cast<T>(1.4)); setHighMidFrequencyHz(static_cast<T>(1200)); setHighMidQ(static_cast<T>(2.5));
        setGateEnabled(true); setGateThresholdDb(static_cast<T>(-60)); gate_.setAttackMs(static_cast<T>(0.5)); gate_.setHoldMs(static_cast<T>(45)); setGateReleaseMs(static_cast<T>(80));
        setVoiceMode(ChainsawVoiceMode::ClassicSwedish); updateAllFilters();
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    bool bypassed_ { false };
    bool gateEnabled_ { true };
    T gateThresholdDb_ { static_cast<T>(-60) };
    T preBoostGainDb_ { static_cast<T>(6) };
    T preBoostFrequencyHz_ { static_cast<T>(1000) };
    T preBoostQ_ { static_cast<T>(2) };
    T stage1Drive_ { static_cast<T>(0.55) };
    T stage2Drive_ { static_cast<T>(0.9) };
    T hardThreshold_ { static_cast<T>(0.3) };
    T lowMidGainDb_ { static_cast<T>(12) };
    T lowMidFrequencyHz_ { static_cast<T>(400) };
    T lowMidQ_ { static_cast<T>(1.4) };
    T highMidGainDb_ { static_cast<T>(15) };
    T highMidFrequencyHz_ { static_cast<T>(1200) };
    T highMidQ_ { static_cast<T>(2.5) };
    T fizzCutHz_ { static_cast<T>(6000) };
    T tightLowCutHz_ { static_cast<T>(80) };
    PedalOversamplingMode oversamplingMode_ { PedalOversamplingMode::x8 };
    PedalQualityMode qualityMode_ { PedalQualityMode::Normal };
    ChainsawVoiceMode voiceMode_ { ChainsawVoiceMode::ClassicSwedish };
    PedalGainStage<T> inputGain_ {};
    PedalGainStage<T> interstageGain_ {};
    PedalGainStage<T> outputGain_ {};
    PedalMix<T> mix_ {};
    PedalClipper<T> stage1_ {};
    PedalClipper<T> stage2_ {};
    filters::DCBlocker<T> dcBlocker_ {};
    dynamics::NoiseGate<T> gate_ {};
    filters::Biquad<T> tightLowCut_ {};
    filters::Biquad<T> preBoost_ {};
    filters::Biquad<T> lowMid_ {};
    filters::Biquad<T> highMid_ {};
    filters::Biquad<T> fizzCut_ {};
    Oversampling<T, 2> oversampling2x_ {};
    Oversampling<T, 4> oversampling4x_ {};
    Oversampling<T, 8> oversampling8x_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_CHAINSAWMETALDSP_HPP
