#ifndef CVDSP_REVERB_PLATEREVERB_HPP
#define CVDSP_REVERB_PLATEREVERB_HPP

/**
 * @file PlateReverb.hpp
 * @brief EMT-inspired plate reverb based on a compact 4x4 FDN.
 *
 * Header-only
 * C++20
 * Real-time safe after prepare()
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "../Core/AudioBufferView.hpp"
#include "../Core/ProcessContext.hpp"
#include "../Delay/DelayLine.hpp"
#include "../Filters/Biquad.hpp"
#include "../Filters/OnePoleFilter.hpp"

namespace cvdsp::reverb
{

/**
 * @brief Bright, dense plate reverb inspired by classic EMT-style plates.
 *
 * Topology:
 *
 * input -> high-shelf brightness filter -> dedicated pre-delay
 *       -> 4x4 Hadamard feedback delay network -> stereo output matrix
 *
 * Each FDN branch uses a fixed-size cvdsp::delay::DelayLine<T> and a
 * cvdsp::filters::LowPassOnePole<T> in the feedback path for high-frequency
 * damping. The input brightness stage uses cvdsp::filters::Biquad<T> as a
 * high-shelf EQ. All storage is fixed-size; the audio loop performs no heap
 * allocation, no resizing, and no RTTI-dependent work.
 */
template<typename T>
class PlateReverb
{
    static_assert(
        std::is_floating_point_v<T>,
        "PlateReverb requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr PlateReverb() noexcept = default;

    /**
     * @brief Prepare the plate reverb for processing.
     */
    void prepare(
        const cvdsp::ProcessContext<T>& context) noexcept
    {
        sampleRate_ =
            std::max(
                context.sampleRate,
                static_cast<T>(1));

        numChannels_ =
            std::clamp<size_type>(
                context.numChannels,
                1u,
                static_cast<size_type>(2));

        const auto delaySampleRate =
            static_cast<typename TankDelay::size_type>(sampleRate_);

        preDelay_.prepare(delaySampleRate);

        for (auto& line : lines_)
        {
            line.delay.prepare(delaySampleRate);
            line.damping.prepare(delaySampleRate);
        }

        brightnessFilter_.prepare(sampleRate_);
        brightnessFilter_.setType(cvdsp::filters::BiquadType::HighShelf);
        brightnessFilter_.setFrequency(static_cast<T>(4200));
        brightnessFilter_.setQ(static_cast<T>(0.7071067811865476));

        updateBrightnessFilter();
        updateDampingFilters();
        updatePreDelaySamples();
        updateDelayTimes();
        updateFeedbackGain();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear all internal delay and filter states.
     */
    void reset() noexcept
    {
        preDelay_.reset();
        brightnessFilter_.reset();

        for (auto& line : lines_)
        {
            line.delay.reset();
            line.damping.reset();
        }

        updatePreDelaySamples();
        updateDelayTimes();
    }

    /**
     * @brief Process a non-interleaved audio buffer in place.
     */
    void processBlock(
        cvdsp::AudioBufferView<T>& buffer) noexcept
    {
        if (!prepared_ || buffer.empty())
        {
            return;
        }

        const size_type channelsToProcess =
            std::min<size_type>(
                buffer.getNumChannels(),
                numChannels_);

        const size_type numSamples =
            buffer.getNumSamples();

        for (size_type sample = 0; sample < numSamples; ++sample)
        {
            if (channelsToProcess == 1)
            {
                T* channel = buffer.getChannel(0);
                const T dryInput = channel[sample];

                T wetLeft = static_cast<T>(0);
                T wetRight = static_cast<T>(0);
                processSample(dryInput, wetLeft, wetRight);

                const T wetMono =
                    (wetLeft + wetRight)
                    * static_cast<T>(0.5);

                channel[sample] = mixOutput(dryInput, wetMono);
            }
            else
            {
                T* left = buffer.getChannel(0);
                T* right = buffer.getChannel(1);

                const T dryLeft = left[sample];
                const T dryRight = right[sample];
                const T monoInput =
                    (dryLeft + dryRight)
                    * static_cast<T>(0.5);

                T wetLeft = static_cast<T>(0);
                T wetRight = static_cast<T>(0);
                processSample(monoInput, wetLeft, wetRight);

                left[sample] = mixOutput(dryLeft, wetLeft);
                right[sample] = mixOutput(dryRight, wetRight);
            }
        }
    }

    /**
     * @brief Set plate decay amount.
     * @param decay Normalized range 0..1. Larger values produce a longer tail.
     */
    void setDecay(
        T decay) noexcept
    {
        decay_ =
            std::clamp(
                decay,
                static_cast<T>(0),
                static_cast<T>(1));

        updateFeedbackGain();
    }

    /**
     * @brief Set high-frequency damping in the feedback network.
     * @param damping Range 0..1. Larger values darken the tail faster.
     */
    void setDamping(
        T damping) noexcept
    {
        damping_ =
            std::clamp(
                damping,
                static_cast<T>(0),
                static_cast<T>(1));

        updateDampingFilters();
    }

    /**
     * @brief Set input brightness / high-shelf emphasis.
     * @param brightness Range 0..1. Larger values increase plate brilliance.
     */
    void setBrightness(
        T brightness) noexcept
    {
        brightness_ =
            std::clamp(
                brightness,
                static_cast<T>(0),
                static_cast<T>(1));

        updateBrightnessFilter();
    }

    /**
     * @brief Set pre-delay before the plate tank.
     * @param preDelayMilliseconds Range 0..120 ms.
     */
    void setPreDelay(
        T preDelayMilliseconds) noexcept
    {
        preDelayMs_ =
            std::clamp(
                preDelayMilliseconds,
                static_cast<T>(0),
                static_cast<T>(120));

        updatePreDelaySamples();
    }

    /**
     * @brief Set wet output gain.
     * @param wet Linear gain in the range 0..1.
     */
    void setWet(
        T wet) noexcept
    {
        wet_ =
            std::clamp(
                wet,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Set dry output gain.
     * @param dry Linear gain in the range 0..1.
     */
    void setDry(
        T dry) noexcept
    {
        dry_ =
            std::clamp(
                dry,
                static_cast<T>(0),
                static_cast<T>(1));
    }

private:

    static constexpr size_type kNumLines = 4;
    static constexpr size_type kMaxTankDelaySamples = 32768;
    static constexpr size_type kMaxPreDelaySamples = 16384;

    using TankDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxTankDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    using PreDelayLine =
        cvdsp::delay::DelayLine<
            T,
            kMaxPreDelaySamples,
            cvdsp::delay::InterpolationType::Linear>;

    struct FDNLine
    {
        TankDelay delay{};
        cvdsp::filters::LowPassOnePole<T> damping{};
        size_type delaySamples = 1;
    };

    static constexpr std::array<T, kNumLines> kBaseDelayMs{
        static_cast<T>(31.10),
        static_cast<T>(37.70),
        static_cast<T>(43.70),
        static_cast<T>(53.90)};

    [[nodiscard]]
    size_type millisecondsToTankSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(kMaxTankDelaySamples - 1)));
    }

    [[nodiscard]]
    size_type millisecondsToPreDelaySamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(0),
                static_cast<T>(kMaxPreDelaySamples - 1)));
    }

    void updateDelayTimes() noexcept
    {
        const T scale =
            static_cast<T>(0.85)
            + (decay_ * static_cast<T>(0.35));

        for (size_type index = 0; index < kNumLines; ++index)
        {
            const T metallicSpread =
                static_cast<T>(index) * static_cast<T>(0.41);

            lines_[index].delaySamples =
                millisecondsToTankSamples(
                    (kBaseDelayMs[index] * scale)
                    + metallicSpread);
        }
    }

    void updateFeedbackGain() noexcept
    {
        feedbackGain_ =
            static_cast<T>(0.58)
            + (decay_ * static_cast<T>(0.36));

        updateDelayTimes();
    }

    void updateDampingFilters() noexcept
    {
        const T minimumCutoff = static_cast<T>(2400);
        const T maximumCutoff = static_cast<T>(20000);
        const T shapedDamping = damping_ * damping_;
        const T cutoff =
            maximumCutoff
            + ((minimumCutoff - maximumCutoff) * shapedDamping);

        for (auto& line : lines_)
        {
            line.damping.setCutoffHz(cutoff);
        }
    }

    void updateBrightnessFilter() noexcept
    {
        const T gainDb =
            static_cast<T>(-1.5)
            + (brightness_ * static_cast<T>(7.5));

        const T frequency =
            static_cast<T>(2600)
            + (brightness_ * static_cast<T>(2800));

        brightnessFilter_.setType(cvdsp::filters::BiquadType::HighShelf);
        brightnessFilter_.setFrequency(frequency);
        brightnessFilter_.setQ(static_cast<T>(0.7071067811865476));
        brightnessFilter_.setGainDB(gainDb);
        brightnessFilter_.updateCoefficients();
    }

    void updatePreDelaySamples() noexcept
    {
        preDelaySamples_ =
            millisecondsToPreDelaySamples(preDelayMs_);
    }

    void processSample(
        T input,
        T& wetLeft,
        T& wetRight) noexcept
    {
        const T brightInput = brightnessFilter_.process(input);

        const T tankInput =
            preDelay_.readSamples(
                static_cast<T>(preDelaySamples_));

        preDelay_.write(brightInput);

        const T delayed0 =
            lines_[0].delay.readIntegerSamples(lines_[0].delaySamples);
        const T delayed1 =
            lines_[1].delay.readIntegerSamples(lines_[1].delaySamples);
        const T delayed2 =
            lines_[2].delay.readIntegerSamples(lines_[2].delaySamples);
        const T delayed3 =
            lines_[3].delay.readIntegerSamples(lines_[3].delaySamples);

        const T damped0 = lines_[0].damping.process(delayed0);
        const T damped1 = lines_[1].damping.process(delayed1);
        const T damped2 = lines_[2].damping.process(delayed2);
        const T damped3 = lines_[3].damping.process(delayed3);

        constexpr T kMatrixScale = static_cast<T>(0.5);
        constexpr T kInputGain = static_cast<T>(0.42);

        const T feedback0 =
            (damped0 + damped1 + damped2 + damped3)
            * kMatrixScale;

        const T feedback1 =
            (damped0 - damped1 + damped2 - damped3)
            * kMatrixScale;

        const T feedback2 =
            (damped0 + damped1 - damped2 - damped3)
            * kMatrixScale;

        const T feedback3 =
            (damped0 - damped1 - damped2 + damped3)
            * kMatrixScale;

        const T inputGain = tankInput * kInputGain;

        lines_[0].delay.write(inputGain + (feedback0 * feedbackGain_));
        lines_[1].delay.write(inputGain + (feedback1 * feedbackGain_));
        lines_[2].delay.write(inputGain + (feedback2 * feedbackGain_));
        lines_[3].delay.write(inputGain + (feedback3 * feedbackGain_));

        wetLeft =
            ((delayed0 - delayed1)
             + (delayed2 * static_cast<T>(0.62))
             - (delayed3 * static_cast<T>(0.38)))
            * static_cast<T>(0.48);

        wetRight =
            ((delayed1 - delayed2)
             + (delayed3 * static_cast<T>(0.62))
             - (delayed0 * static_cast<T>(0.38)))
            * static_cast<T>(0.48);
    }

    [[nodiscard]]
    T mixOutput(
        T dryInput,
        T wetInput) const noexcept
    {
        return
            (dryInput * dry_)
            + (wetInput * wet_);
    }

    PreDelayLine preDelay_{};
    std::array<FDNLine, kNumLines> lines_{};
    cvdsp::filters::Biquad<T> brightnessFilter_{};

    T sampleRate_ = static_cast<T>(44100);
    T decay_ = static_cast<T>(0.62);
    T damping_ = static_cast<T>(0.28);
    T brightness_ = static_cast<T>(0.72);
    T preDelayMs_ = static_cast<T>(18);
    T wet_ = static_cast<T>(0.24);
    T dry_ = static_cast<T>(1);
    T feedbackGain_ = static_cast<T>(0.80);

    size_type preDelaySamples_ = 0;
    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_PLATEREVERB_HPP
