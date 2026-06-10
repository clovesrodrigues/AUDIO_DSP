#ifndef CVDSP_GUITAR_PEDALS_PEDALPREFILTER_HPP
#define CVDSP_GUITAR_PEDALS_PEDALPREFILTER_HPP

/**
 * @file PedalPreFilter.hpp
 * @brief Pre-distortion voice/EQ stage for CV_DSP guitar pedals.
 */

#include <algorithm>
#include <type_traits>

#include "PedalParameterUtils.hpp"
#include "../../Filters/Biquad.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T = float>
class PedalPreFilter
{
    static_assert(std::is_floating_point_v<T>, "PedalPreFilter requires a floating point type");

public:
    constexpr PedalPreFilter() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        highPassA_.prepare(sampleRate_);
        highPassB_.prepare(sampleRate_);
        lowPass_.prepare(sampleRate_);
        bassShelf_.prepare(sampleRate_);
        lowMid_.prepare(sampleRate_);
        mid_.prepare(sampleRate_);
        trebleShelf_.prepare(sampleRate_);
        presence_.prepare(sampleRate_);
        updateAll();
        reset();
    }

    void reset() noexcept
    {
        highPassA_.reset();
        highPassB_.reset();
        lowPass_.reset();
        bassShelf_.reset();
        lowMid_.reset();
        mid_.reset();
        trebleShelf_.reset();
        presence_.reset();
    }

    void setHighPassFrequency(T hz) noexcept { highPassFrequency_ = clampFrequency(hz); updateHighPass(); }
    void setHighPassSlopeIndex(int slopeIndex) noexcept { highPassSlopeIndex_ = std::clamp(slopeIndex, 0, 2); }
    void setLowPassFrequency(T hz) noexcept { lowPassFrequency_ = clampFrequency(hz); updateLowPass(); }
    void setBassGainDb(T db) noexcept { bassGainDb_ = clampEqGain(db); updateBass(); }
    void setLowMidFrequency(T hz) noexcept { lowMidFrequency_ = clampFrequency(hz); updateLowMid(); }
    void setLowMidGainDb(T db) noexcept { lowMidGainDb_ = clampEqGain(db); updateLowMid(); }
    void setLowMidQ(T q) noexcept { lowMidQ_ = clampQ(q); updateLowMid(); }
    void setMidFrequency(T hz) noexcept { midFrequency_ = clampFrequency(hz); updateMid(); }
    void setMidGainDb(T db) noexcept { midGainDb_ = clampEqGain(db); updateMid(); }
    void setMidQ(T q) noexcept { midQ_ = clampQ(q); updateMid(); }
    void setTrebleGainDb(T db) noexcept { trebleGainDb_ = clampEqGain(db); updateTreble(); }
    void setPresenceFrequency(T hz) noexcept { presenceFrequency_ = clampFrequency(hz); updatePresence(); }
    void setPresenceGainDb(T db) noexcept { presenceGainDb_ = clampEqGain(db); updatePresence(); }

    void setVoiceMode(PedalToneMode mode) noexcept
    {
        voiceMode_ = mode;
        applyVoiceMode();
    }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        T y = input;
        y = highPassA_.process(y);
        if (highPassSlopeIndex_ >= 1)
            y = highPassB_.process(y);
        y = lowPass_.process(y);
        y = bassShelf_.process(y);
        y = lowMid_.process(y);
        y = mid_.process(y);
        y = trebleShelf_.process(y);
        y = presence_.process(y);
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
        const T nyquistSafe = sampleRate_ * static_cast<T>(0.49);
        const T maximum = std::min(PedalConstants<T>::kMaxFrequencyHz, nyquistSafe);
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
        updateHighPass();
        updateLowPass();
        updateBass();
        updateLowMid();
        updateMid();
        updateTreble();
        updatePresence();
    }

    void updateHighPass() noexcept
    {
        configure(highPassA_, filters::BiquadType::HighPass, highPassFrequency_, PedalConstants<T>::kDefaultQ);
        configure(highPassB_, filters::BiquadType::HighPass, highPassFrequency_, PedalConstants<T>::kDefaultQ);
    }

    void updateLowPass() noexcept
    {
        configure(lowPass_, filters::BiquadType::LowPass, lowPassFrequency_, PedalConstants<T>::kDefaultQ);
    }

    void updateBass() noexcept
    {
        configure(bassShelf_, filters::BiquadType::LowShelf, static_cast<T>(120), PedalConstants<T>::kDefaultQ, bassGainDb_);
    }

    void updateLowMid() noexcept
    {
        configure(lowMid_, filters::BiquadType::PeakingEQ, lowMidFrequency_, lowMidQ_, lowMidGainDb_);
    }

    void updateMid() noexcept
    {
        configure(mid_, filters::BiquadType::PeakingEQ, midFrequency_, midQ_, midGainDb_);
    }

    void updateTreble() noexcept
    {
        configure(trebleShelf_, filters::BiquadType::HighShelf, static_cast<T>(2500), PedalConstants<T>::kDefaultQ, trebleGainDb_);
    }

    void updatePresence() noexcept
    {
        configure(presence_, filters::BiquadType::PeakingEQ, presenceFrequency_, static_cast<T>(1.2), presenceGainDb_);
    }

    void applyVoiceMode() noexcept
    {
        switch (voiceMode_)
        {
            case PedalToneMode::Dark:
                trebleGainDb_ = static_cast<T>(-4); presenceGainDb_ = static_cast<T>(-3); lowPassFrequency_ = static_cast<T>(5000); break;
            case PedalToneMode::Bright:
                trebleGainDb_ = static_cast<T>(3); presenceGainDb_ = static_cast<T>(4); lowPassFrequency_ = static_cast<T>(12000); break;
            case PedalToneMode::Scooped:
                lowMidGainDb_ = static_cast<T>(-3); midGainDb_ = static_cast<T>(-6); break;
            case PedalToneMode::MidForward:
                midGainDb_ = static_cast<T>(5); presenceGainDb_ = static_cast<T>(2); break;
            case PedalToneMode::Flat:
            case PedalToneMode::Neutral:
            default:
                bassGainDb_ = static_cast<T>(0); lowMidGainDb_ = static_cast<T>(0); midGainDb_ = static_cast<T>(0);
                trebleGainDb_ = static_cast<T>(0); presenceGainDb_ = static_cast<T>(0); lowPassFrequency_ = static_cast<T>(18000); break;
        }
        updateAll();
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    T highPassFrequency_ { static_cast<T>(80) };
    T lowPassFrequency_ { static_cast<T>(18000) };
    T bassGainDb_ { static_cast<T>(0) };
    T lowMidFrequency_ { static_cast<T>(300) };
    T lowMidGainDb_ { static_cast<T>(0) };
    T lowMidQ_ { static_cast<T>(1) };
    T midFrequency_ { static_cast<T>(900) };
    T midGainDb_ { static_cast<T>(0) };
    T midQ_ { static_cast<T>(1) };
    T trebleGainDb_ { static_cast<T>(0) };
    T presenceFrequency_ { static_cast<T>(2500) };
    T presenceGainDb_ { static_cast<T>(0) };
    int highPassSlopeIndex_ { 0 };
    PedalToneMode voiceMode_ { PedalToneMode::Neutral };
    filters::Biquad<T> highPassA_ {};
    filters::Biquad<T> highPassB_ {};
    filters::Biquad<T> lowPass_ {};
    filters::Biquad<T> bassShelf_ {};
    filters::Biquad<T> lowMid_ {};
    filters::Biquad<T> mid_ {};
    filters::Biquad<T> trebleShelf_ {};
    filters::Biquad<T> presence_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALPREFILTER_HPP
