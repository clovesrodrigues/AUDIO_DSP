#ifndef CVDSP_GUITAR_PEDALS_PEDALDRIVECORE_HPP
#define CVDSP_GUITAR_PEDALS_PEDALDRIVECORE_HPP

/**
 * @file PedalDriveCore.hpp
 * @brief Shared distortion pedal signal chain with optional oversampling.
 */

#include <cstddef>
#include <type_traits>

#include "PedalClipper.hpp"
#include "PedalGainStage.hpp"
#include "PedalMix.hpp"
#include "PedalPostFilter.hpp"
#include "PedalPreFilter.hpp"
#include "../../Core/AudioBufferView.hpp"
#include "../../Filters/DCBlocker.hpp"
#include "../../Math/Oversampling.hpp"

namespace cvdsp::guitar::pedals
{

template<typename T = float>
class PedalDriveCore
{
    static_assert(std::is_floating_point_v<T>, "PedalDriveCore requires a floating point type");

public:
    constexpr PedalDriveCore() noexcept = default;

    void prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : PedalConstants<T>::kDefaultSampleRate;
        inputGain_.prepare(sampleRate_);
        outputGain_.prepare(sampleRate_);
        mix_.prepare(sampleRate_);
        preFilter_.prepare(sampleRate_);
        postFilter_.prepare(sampleRate_);
        clipper_.prepare(sampleRate_ * static_cast<T>(8));
        dcBlocker_.prepare(sampleRate_ * static_cast<T>(8));
        dcBlocker_.setCutoffHz(static_cast<T>(5));
        oversampling2x_.prepare(sampleRate_);
        oversampling4x_.prepare(sampleRate_);
        oversampling8x_.prepare(sampleRate_);
        reset();
    }

    void reset() noexcept
    {
        inputGain_.reset();
        outputGain_.reset();
        mix_.reset();
        preFilter_.reset();
        postFilter_.reset();
        clipper_.reset();
        dcBlocker_.reset();
        oversampling2x_.reset();
        oversampling4x_.reset();
        oversampling8x_.reset();
    }

    void setBypassed(bool enabled) noexcept { bypassed_ = enabled; }
    void setOversamplingMode(PedalOversamplingMode mode) noexcept { oversamplingMode_ = mode; }

    void setQualityMode(PedalQualityMode mode) noexcept
    {
        qualityMode_ = mode;
        clipper_.setQualityMode(mode);
    }

    void setInputGainDb(T db) noexcept { inputGain_.setGainDb(db); }
    void setDriveDb(T db) noexcept { clipper_.setDriveDb(db); }
    void setOutputGainDb(T db) noexcept { outputGain_.setGainDb(db); }
    void setDryWetMix(T normalized) noexcept { mix_.setMix(normalized); }
    void setPhaseInvert(bool enabled) noexcept { mix_.setPhaseInvert(enabled); }

    [[nodiscard]] PedalPreFilter<T>& preFilter() noexcept { return preFilter_; }
    [[nodiscard]] const PedalPreFilter<T>& preFilter() const noexcept { return preFilter_; }
    [[nodiscard]] PedalClipper<T>& clipper() noexcept { return clipper_; }
    [[nodiscard]] const PedalClipper<T>& clipper() const noexcept { return clipper_; }
    [[nodiscard]] PedalPostFilter<T>& postFilter() noexcept { return postFilter_; }
    [[nodiscard]] const PedalPostFilter<T>& postFilter() const noexcept { return postFilter_; }

    [[nodiscard]] inline T processSample(T input) noexcept
    {
        if (bypassed_)
            return input;

        const T dry = input;
        T wet = inputGain_.processSample(input);
        wet = preFilter_.processSample(wet);
        wet = processNonlinear(wet);
        wet = postFilter_.processSample(wet);
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

private:
    [[nodiscard]] inline T processNonlinear(T input) noexcept
    {
        switch (oversamplingMode_)
        {
            case PedalOversamplingMode::x2: return processOversampled(oversampling2x_, input);
            case PedalOversamplingMode::x4: return processOversampled(oversampling4x_, input);
            case PedalOversamplingMode::x8: return processOversampled(oversampling8x_, input);
            case PedalOversamplingMode::Off:
            default:
                return dcBlocker_.process(clipper_.processSample(input));
        }
    }

    template<std::size_t Factor>
    [[nodiscard]] inline T processOversampled(Oversampling<T, Factor>& oversampler, T input) noexcept
    {
        auto block = oversampler.processUp(input);
        for (std::size_t i = 0; i < Factor; ++i)
            block[i] = dcBlocker_.process(clipper_.processSample(block[i]));
        return oversampler.processDown(block);
    }

    T sampleRate_ { PedalConstants<T>::kDefaultSampleRate };
    bool bypassed_ { false };
    PedalOversamplingMode oversamplingMode_ { PedalOversamplingMode::x4 };
    PedalQualityMode qualityMode_ { PedalQualityMode::Normal };
    PedalGainStage<T> inputGain_ {};
    PedalGainStage<T> outputGain_ {};
    PedalMix<T> mix_ {};
    PedalPreFilter<T> preFilter_ {};
    PedalClipper<T> clipper_ {};
    PedalPostFilter<T> postFilter_ {};
    filters::DCBlocker<T> dcBlocker_ {};
    Oversampling<T, 2> oversampling2x_ {};
    Oversampling<T, 4> oversampling4x_ {};
    Oversampling<T, 8> oversampling8x_ {};
};

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALDRIVECORE_HPP
