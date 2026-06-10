#ifndef CVDSP_GUITAR_PEDALS_PEDALPOSTFILTER_HPP
#define CVDSP_GUITAR_PEDALS_PEDALPOSTFILTER_HPP

/**
 * @file PedalPostFilter.hpp
 * @brief Post-distortion tone/EQ stage for CV_DSP guitar pedals.
 */

#include <algorithm>
#include <type_traits>

#include "PedalParameterUtils.hpp"
#include "../../Filters/Biquad.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T = float>
class PedalPostFilter
{
    static_assert(std::is_floating_point_v<T>, "PedalPostFilter requires a floating point type");

public:
    constexpr PedalPostFilter() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        highPass_.prepare(sampleRate_);
        lowPassA_.prepare(sampleRate_);
        lowPassB_.prepare(sampleRate_);
        bassShelf_.prepare(sampleRate_);
        middle_.prepare(sampleRate_);
        trebleShelf_.prepare(sampleRate_);
        presence_.prepare(sampleRate_);
        fizzCut_.prepare(sampleRate_);
        notch_.prepare(sampleRate_);
        updateAll();
        reset();
    }

    void reset() noexcept
    {
        highPass_.reset();
        lowPassA_.reset();
        lowPassB_.reset();
        bassShelf_.reset();
        middle_.reset();
        trebleShelf_.reset();
        presence_.reset();
        fizzCut_.reset();
        notch_.reset();
    }

    void setTone(T normalized) noexcept { tone_ = clamp01(normalized); applyToneMacro(); }
    void setHighPassFrequency(T hz) noexcept { highPassFrequency_ = clampFrequency(hz); updateHighPass(); }
    void setLowPassFrequency(T hz) noexcept { lowPassFrequency_ = clampFrequency(hz); updateLowPass(); }
    void setLowPassSlopeIndex(int slopeIndex) noexcept { lowPassSlopeIndex_ = std::clamp(slopeIndex, 0, 2); }
    void setBassGainDb(T db) noexcept { bassGainDb_ = clampEqGain(db); updateBass(); }
    void setMiddleGainDb(T db) noexcept { middleGainDb_ = clampEqGain(db); updateMiddle(); }
    void setMidFrequency(T hz) noexcept { midFrequency_ = clampFrequency(hz); updateMiddle(); }
    void setMidQ(T q) noexcept { midQ_ = clampQ(q); updateMiddle(); }
    void setTrebleGainDb(T db) noexcept { trebleGainDb_ = clampEqGain(db); updateTreble(); }
    void setPresenceGainDb(T db) noexcept { presenceGainDb_ = clampEqGain(db); updatePresence(); }
    void setPresenceFrequency(T hz) noexcept { presenceFrequency_ = clampFrequency(hz); updatePresence(); }
    void setFizzCutFrequency(T hz) noexcept { fizzCutFrequency_ = clampFrequency(hz); updateFizzCut(); }
    void setNotchFrequency(T hz) noexcept { notchFrequency_ = clampFrequency(hz); updateNotch(); }
    void setNotchDepthDb(T db) noexcept { notchDepthDb_ = std::clamp(db, static_cast<T>(-36), static_cast<T>(0)); updateNotch(); }
    void setNotchQ(T q) noexcept { notchQ_ = clampQ(q); updateNotch(); }

    void setToneMode(PedalToneMode mode) noexcept
    {
        toneMode_ = mode;
        applyToneMode();
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        T y = input;
        y = highPass_.process(y);
        y = bassShelf_.process(y);
        y = middle_.process(y);
        y = trebleShelf_.process(y);
        y = presence_.process(y);
        if (notchDepthDb_ < static_cast<T>(-0.001))
            y = notch_.process(y);
        y = fizzCut_.process(y);
        y = lowPassA_.process(y);
        if (lowPassSlopeIndex_ >= 1)
            y = lowPassB_.process(y);
        return y;
    }

private:
    [[nodiscard]] static constexpr T clampEqGain(T db) noexcept
    {
        return db < static_cast<T>(-24) ? static_cast<T>(-24) : (db > static_cast<T>(24) ? static_cast<T>(24) : db);
    }

    [[nodiscard]] static constexpr T clampQ(T q) noexcept
    {
        return q < PedalConstants<T>::kMinQ ? PedalConstants<T>::kMinQ : (q > PedalConstants<T>::kMaxQ ? PedalConstants<T>::kMaxQ : q);
    }

    [[nodiscard]] T clampFrequency(T hz) const noexcept
    {
        const T maximum = std::min(PedalConstants<T>::kMaxFrequencyHz, sampleRate_ * static_cast<T>(0.49));
        return std::clamp(hz, PedalConstants<T>::kMinFrequencyHz, maximum);
    }

    void configure(filters::Biquad<T>& filter, filters::BiquadType type, T frequency, T q, T gainDb = static_cast<T>(0)) noexcept
    {
        filter.setType(type);
        filter.setFrequency(clampFrequency(frequency));
        filter.setQ(clampQ(q));
        filter.setGainDB(gainDb);
        filter.updateCoefficients();
    }

    void updateAll() noexcept
    {
        updateHighPass(); updateLowPass(); updateBass(); updateMiddle(); updateTreble(); updatePresence(); updateFizzCut(); updateNotch();
    }

    void updateHighPass() noexcept { configure(highPass_, filters::BiquadType::HighPass, highPassFrequency_, PedalConstants<T>::kDefaultQ); }
    void updateLowPass() noexcept
    {
        configure(lowPassA_, filters::BiquadType::LowPass, lowPassFrequency_, PedalConstants<T>::kDefaultQ);
        configure(lowPassB_, filters::BiquadType::LowPass, lowPassFrequency_, PedalConstants<T>::kDefaultQ);
    }
    void updateBass() noexcept { configure(bassShelf_, filters::BiquadType::LowShelf, static_cast<T>(120), PedalConstants<T>::kDefaultQ, bassGainDb_); }
    void updateMiddle() noexcept { configure(middle_, filters::BiquadType::PeakingEQ, midFrequency_, midQ_, middleGainDb_); }
    void updateTreble() noexcept { configure(trebleShelf_, filters::BiquadType::HighShelf, static_cast<T>(3000), PedalConstants<T>::kDefaultQ, trebleGainDb_); }
    void updatePresence() noexcept { configure(presence_, filters::BiquadType::PeakingEQ, presenceFrequency_, static_cast<T>(1.2), presenceGainDb_); }
    void updateFizzCut() noexcept { configure(fizzCut_, filters::BiquadType::LowPass, fizzCutFrequency_, static_cast<T>(0.55)); }
    void updateNotch() noexcept { configure(notch_, filters::BiquadType::PeakingEQ, notchFrequency_, notchQ_, notchDepthDb_); }

    void applyToneMacro() noexcept
    {
        lowPassFrequency_ = normalizedToLogFrequency(static_cast<T>(1000), static_cast<T>(12000), tone_);
        presenceGainDb_ = normalizedToLinear(static_cast<T>(-3), static_cast<T>(5), tone_);
        updateLowPass();
        updatePresence();
    }

    void applyToneMode() noexcept
    {
        switch (toneMode_)
        {
            case PedalToneMode::Dark:
                tone_ = static_cast<T>(0.25); trebleGainDb_ = static_cast<T>(-4); presenceGainDb_ = static_cast<T>(-3); break;
            case PedalToneMode::Bright:
                tone_ = static_cast<T>(0.75); trebleGainDb_ = static_cast<T>(4); presenceGainDb_ = static_cast<T>(5); break;
            case PedalToneMode::Scooped:
                middleGainDb_ = static_cast<T>(-6); midFrequency_ = static_cast<T>(450); midQ_ = static_cast<T>(1.2); break;
            case PedalToneMode::MidForward:
                middleGainDb_ = static_cast<T>(5); midFrequency_ = static_cast<T>(900); midQ_ = static_cast<T>(1); break;
            case PedalToneMode::Flat:
            case PedalToneMode::Neutral:
            default:
                bassGainDb_ = middleGainDb_ = trebleGainDb_ = presenceGainDb_ = static_cast<T>(0); break;
        }
        applyToneMacro();
        updateAll();
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T tone_ { static_cast<T>(0.5) };
    T highPassFrequency_ { static_cast<T>(40) };
    T lowPassFrequency_ { static_cast<T>(6000) };
    T bassGainDb_ { static_cast<T>(0) };
    T middleGainDb_ { static_cast<T>(0) };
    T midFrequency_ { static_cast<T>(800) };
    T midQ_ { static_cast<T>(1) };
    T trebleGainDb_ { static_cast<T>(0) };
    T presenceGainDb_ { static_cast<T>(0) };
    T presenceFrequency_ { static_cast<T>(3000) };
    T fizzCutFrequency_ { static_cast<T>(9000) };
    T notchFrequency_ { static_cast<T>(400) };
    T notchDepthDb_ { static_cast<T>(0) };
    T notchQ_ { static_cast<T>(2) };
    int lowPassSlopeIndex_ { 0 };
    PedalToneMode toneMode_ { PedalToneMode::Neutral };
    filters::Biquad<T> highPass_ {};
    filters::Biquad<T> lowPassA_ {};
    filters::Biquad<T> lowPassB_ {};
    filters::Biquad<T> bassShelf_ {};
    filters::Biquad<T> middle_ {};
    filters::Biquad<T> trebleShelf_ {};
    filters::Biquad<T> presence_ {};
    filters::Biquad<T> fizzCut_ {};
    filters::Biquad<T> notch_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALPOSTFILTER_HPP
