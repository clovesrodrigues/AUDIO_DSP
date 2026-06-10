#ifndef CVDSP_REVERB_HALLREVERB_HPP
#define CVDSP_REVERB_HALLREVERB_HPP

/**
 * @file HallReverb.hpp
 * @brief Low-allocation extended Schroeder hall reverb for CV_DSP.
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
#include "../Filters/OnePoleFilter.hpp"

namespace cvdsp::reverb
{

/**
 * @brief Extended Schroeder reverb tuned for large concert halls.
 *
 * Signal path per stereo tank:
 *
 * input -> dedicated pre-delay -> 8 parallel feedback comb filters
 *       -> 4 serial all-pass diffusors -> wet stereo width matrix
 *
 * The feedback comb filters use cvdsp::delay::DelayLine<T> for fixed-size delay
 * storage and cvdsp::filters::LowPassOnePole<T> for high-frequency damping in
 * the feedback path. Diffusion is implemented with four
 * compact all-pass diffusors built on cvdsp::delay::DelayLine<T> per tank.
 *
 * The class owns only fixed-size members and performs no heap allocation in the
 * audio loop. processBlock() and all setters are noexcept and real-time safe.
 */
template<typename T>
class HallReverb
{
    static_assert(
        std::is_floating_point_v<T>,
        "HallReverb requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr HallReverb() noexcept = default;

    /**
     * @brief Prepare the hall reverb for processing.
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
                kNumTanks);

        const auto delaySampleRate =
            static_cast<typename CombDelay::size_type>(sampleRate_);

        for (auto& channel : channels_)
        {
            channel.preDelay.prepare(delaySampleRate);

            for (auto& comb : channel.combs)
            {
                comb.delay.prepare(delaySampleRate);
                comb.damping.prepare(delaySampleRate);
            }

            for (auto& diffuser : channel.diffusers)
            {
                diffuser.prepare(delaySampleRate);
            }
        }

        updateDampingFilters();
        updatePreDelaySamples();
        updateDelayTimes();
        updateFeedbackGains();
        updateDiffusionFeedback();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear all internal delay and filter states.
     */
    void reset() noexcept
    {
        for (auto& channel : channels_)
        {
            channel.preDelay.reset();

            for (auto& comb : channel.combs)
            {
                comb.delay.reset();
                comb.damping.reset();
            }

            for (auto& diffuser : channel.diffusers)
            {
                diffuser.reset();
            }
        }

        updatePreDelaySamples();
        updateDelayTimes();
        updateDiffusionFeedback();
    }

    /**
     * @brief Process a non-interleaved buffer view in place.
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
                const T wet = processTank(dryInput, channels_[0]);
                channel[sample] = mixOutput(dryInput, wet);
            }
            else
            {
                T* left = buffer.getChannel(0);
                T* right = buffer.getChannel(1);

                const T dryLeft = left[sample];
                const T dryRight = right[sample];

                const T midDrive =
                    (dryLeft + dryRight)
                    * static_cast<T>(0.5);

                const T sideDrive =
                    (dryLeft - dryRight)
                    * static_cast<T>(0.5);

                const T wetLeftRaw =
                    processTank(
                        (midDrive * static_cast<T>(0.82))
                        + (sideDrive * static_cast<T>(0.18)),
                        channels_[0]);

                const T wetRightRaw =
                    processTank(
                        (midDrive * static_cast<T>(0.82))
                        - (sideDrive * static_cast<T>(0.18)),
                        channels_[1]);

                T wetLeft = static_cast<T>(0);
                T wetRight = static_cast<T>(0);
                applyWidth(wetLeftRaw, wetRightRaw, wetLeft, wetRight);

                left[sample] = mixOutput(dryLeft, wetLeft);
                right[sample] = mixOutput(dryRight, wetRight);
            }
        }
    }

    /**
     * @brief Set normalized hall size.
     * @param hallSize Range 0..1. Larger values increase delay taps and density.
     */
    void setHallSize(
        T hallSize) noexcept
    {
        hallSize_ =
            std::clamp(
                hallSize,
                static_cast<T>(0),
                static_cast<T>(1));

        updateDelayTimes();
        updateFeedbackGains();
        updateDiffusionFeedback();
    }

    /**
     * @brief Set reverberation decay time.
     * @param rt60Seconds RT60 in seconds, clamped to 0.30..12.0 s.
     */
    void setRT60(
        T rt60Seconds) noexcept
    {
        rt60_ =
            std::clamp(
                rt60Seconds,
                static_cast<T>(0.30),
                static_cast<T>(12.0));

        updateFeedbackGains();
    }

    /**
     * @brief Set high-frequency damping amount.
     * @param damping Range 0..1. Larger values produce darker, smoother tails.
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
     * @brief Set dedicated pre-delay before the reverb network.
     * @param preDelayMilliseconds Range 0..250 ms.
     */
    void setPreDelay(
        T preDelayMilliseconds) noexcept
    {
        preDelayMs_ =
            std::clamp(
                preDelayMilliseconds,
                static_cast<T>(0),
                static_cast<T>(250));

        updatePreDelaySamples();
    }

    /**
     * @brief Set stereo field width.
     * @param width Range 0..1. 0 is mono wet, 1 is expanded stereo wet field.
     */
    void setWidth(
        T width) noexcept
    {
        width_ =
            std::clamp(
                width,
                static_cast<T>(0),
                static_cast<T>(1));
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

    static constexpr size_type kNumTanks = 2;
    static constexpr size_type kNumCombs = 8;
    static constexpr size_type kNumDiffusers = 4;
    static constexpr size_type kMaxCombDelaySamples = 32768;

    using CombDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxCombDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    using PreDelayLine =
        cvdsp::delay::DelayLine<
            T,
            cvdsp::delay::DelayLine<T>::getMaxDelaySamples(),
            cvdsp::delay::InterpolationType::Linear>;

    static constexpr size_type kMaxDiffuserDelaySamples = 8192;

    using DiffuserDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxDiffuserDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    struct DiffuserAllPass
    {
        DiffuserDelay delay{};
        size_type delaySamples = 1;
        T feedback = static_cast<T>(0.5);

        void prepare(
            typename DiffuserDelay::size_type sampleRate) noexcept
        {
            delay.prepare(sampleRate);
        }

        void reset() noexcept
        {
            delay.reset();
        }

        void setDelaySamples(
            size_type newDelaySamples) noexcept
        {
            delaySamples =
                std::clamp<size_type>(
                    newDelaySamples,
                    1u,
                    kMaxDiffuserDelaySamples - 1u);
        }

        void setFeedback(
            T newFeedback) noexcept
        {
            feedback =
                std::clamp(
                    newFeedback,
                    static_cast<T>(-0.999),
                    static_cast<T>(0.999));
        }

        [[nodiscard]]
        T process(
            T input) noexcept
        {
            const T delayed =
                delay.readIntegerSamples(delaySamples);

            const T output =
                delayed
                - (feedback * input);

            delay.write(
                input
                + (feedback * output));

            return output;
        }
    };

    struct CombFilter
    {
        CombDelay delay{};
        cvdsp::filters::LowPassOnePole<T> damping{};
        size_type delaySamples = 1;
        T feedback = static_cast<T>(0.1);
    };

    struct ChannelState
    {
        PreDelayLine preDelay{};
        size_type preDelaySamples = 0;
        std::array<CombFilter, kNumCombs> combs{};
        std::array<DiffuserAllPass, kNumDiffusers> diffusers{};
    };

    static constexpr std::array<T, kNumCombs> kBaseCombMs{
        static_cast<T>(29.70),
        static_cast<T>(37.10),
        static_cast<T>(41.10),
        static_cast<T>(43.70),
        static_cast<T>(53.10),
        static_cast<T>(59.30),
        static_cast<T>(67.90),
        static_cast<T>(73.70)};

    static constexpr std::array<T, kNumDiffusers> kBaseAllPassMs{
        static_cast<T>(12.40),
        static_cast<T>(8.30),
        static_cast<T>(5.10),
        static_cast<T>(2.70)};

    [[nodiscard]]
    static T hallScale(
        T hallSize) noexcept
    {
        return
            static_cast<T>(0.75)
            + (hallSize * static_cast<T>(1.15));
    }

    [[nodiscard]]
    size_type millisecondsToCombSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(kMaxCombDelaySamples - 1)));
    }

    [[nodiscard]]
    size_type millisecondsToDelayLineSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(0),
                static_cast<T>(cvdsp::delay::DelayLine<T>::getMaxDelaySamples() - 1)));
    }

    [[nodiscard]]
    size_type millisecondsToDiffuserSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(kMaxDiffuserDelaySamples - 1)));
    }

    void updatePreDelaySamples() noexcept
    {
        for (size_type channel = 0; channel < kNumTanks; ++channel)
        {
            const T channelOffsetMs =
                (channel == 0)
                    ? static_cast<T>(0)
                    : static_cast<T>(1.35);

            channels_[channel].preDelaySamples =
                millisecondsToDelayLineSamples(
                    preDelayMs_ + channelOffsetMs);
        }
    }

    void updateDelayTimes() noexcept
    {
        const T scale = hallScale(hallSize_);

        for (size_type channel = 0; channel < kNumTanks; ++channel)
        {
            const T stereoOffsetMs =
                (channel == 0)
                    ? static_cast<T>(0)
                    : static_cast<T>(0.91);

            const T polaritySpread =
                (channel == 0)
                    ? static_cast<T>(1)
                    : static_cast<T>(-1);

            for (size_type index = 0; index < kNumCombs; ++index)
            {
                const T spreadMs =
                    static_cast<T>((index % 2u) == 0u ? 0.37 : -0.29)
                    * polaritySpread;

                auto& comb = channels_[channel].combs[index];
                comb.delaySamples =
                    millisecondsToCombSamples(
                        (kBaseCombMs[index] * scale)
                        + stereoOffsetMs
                        + spreadMs);
            }

            for (size_type index = 0; index < kNumDiffusers; ++index)
            {
                const size_type delaySamples =
                    millisecondsToDiffuserSamples(
                        (kBaseAllPassMs[index] * scale)
                        + (stereoOffsetMs * static_cast<T>(0.5)));

                channels_[channel].diffusers[index].setDelaySamples(delaySamples);
            }
        }
    }

    void updateFeedbackGains() noexcept
    {
        const T safeRt60 =
            std::max(
                rt60_,
                static_cast<T>(0.30));

        for (auto& channel : channels_)
        {
            for (auto& comb : channel.combs)
            {
                const T delaySeconds =
                    static_cast<T>(comb.delaySamples)
                    / sampleRate_;

                comb.feedback =
                    std::clamp(
                        std::pow(
                            static_cast<T>(10),
                            (static_cast<T>(-3) * delaySeconds) / safeRt60),
                        static_cast<T>(0.08),
                        static_cast<T>(0.93));
            }
        }
    }

    void updateDampingFilters() noexcept
    {
        const T minimumCutoff = static_cast<T>(900);
        const T maximumCutoff = static_cast<T>(18000);
        const T shapedDamping = damping_ * damping_;
        const T cutoff =
            maximumCutoff
            + ((minimumCutoff - maximumCutoff) * shapedDamping);

        for (auto& channel : channels_)
        {
            for (auto& comb : channel.combs)
            {
                comb.damping.setCutoffHz(cutoff);
            }
        }
    }

    void updateDiffusionFeedback() noexcept
    {
        const T baseFeedback =
            static_cast<T>(0.56)
            + (hallSize_ * static_cast<T>(0.10));

        for (auto& channel : channels_)
        {
            for (size_type index = 0; index < kNumDiffusers; ++index)
            {
                const T feedback =
                    baseFeedback
                    - (static_cast<T>(index) * static_cast<T>(0.045));

                channel.diffusers[index].setFeedback(feedback);
            }
        }
    }

    [[nodiscard]]
    T processTank(
        T input,
        ChannelState& channel) noexcept
    {
        const T predelayed =
            channel.preDelay.readSamples(
                static_cast<T>(channel.preDelaySamples));

        channel.preDelay.write(input);

        T combSum = static_cast<T>(0);

        for (auto& comb : channel.combs)
        {
            const T delayed =
                comb.delay.readIntegerSamples(comb.delaySamples);

            const T damped =
                comb.damping.process(delayed);

            comb.delay.write(
                predelayed
                + (damped * comb.feedback));

            combSum += delayed;
        }

        T wet = combSum * static_cast<T>(0.125);

        for (auto& diffuser : channel.diffusers)
        {
            wet = diffuser.process(wet);
        }

        return wet;
    }

    void applyWidth(
        T leftIn,
        T rightIn,
        T& leftOut,
        T& rightOut) const noexcept
    {
        const T mid =
            (leftIn + rightIn)
            * static_cast<T>(0.5);

        const T side =
            (leftIn - rightIn)
            * static_cast<T>(0.5);

        const T sideGain =
            width_ * static_cast<T>(1.45);

        leftOut = mid + (side * sideGain);
        rightOut = mid - (side * sideGain);
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

    std::array<ChannelState, kNumTanks> channels_{};

    T sampleRate_ = static_cast<T>(44100);
    T hallSize_ = static_cast<T>(0.65);
    T rt60_ = static_cast<T>(3.20);
    T damping_ = static_cast<T>(0.35);
    T preDelayMs_ = static_cast<T>(24.0);
    T width_ = static_cast<T>(0.85);
    T wet_ = static_cast<T>(0.24);
    T dry_ = static_cast<T>(1);

    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_HALLREVERB_HPP
