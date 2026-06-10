#ifndef CVDSP_REVERB_ROOMREVERB_HPP
#define CVDSP_REVERB_ROOMREVERB_HPP

/**
 * @file RoomReverb.hpp
 * @brief Low-CPU short room reverb based on the classic Schroeder topology.
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
#include "../Filters/AllPassFilter.hpp"
#include "../Filters/OnePoleFilter.hpp"

namespace cvdsp::reverb
{

/**
 * @brief Compact Schroeder room reverb optimized for short, tight spaces.
 *
 * Signal path per stereo side:
 *
 * input -> 4 parallel feedback comb filters -> sum -> 2 serial all-pass diffusers
 *
 * The comb filters use cvdsp::delay::DelayLine<T> for their delay memory and a
 * cvdsp::filters::LowPassOnePole<T> in the feedback path for damping. The two
 * diffusors are cvdsp::filters::AllPassFilter<T> instances.
 *
 * The class performs no dynamic allocation. prepare() configures and clears the
 * fixed-size internal delay structures; processBlock() and all parameter setters
 * are O(1) per sample and do not allocate.
 */
template<typename T>
class RoomReverb
{
    static_assert(
        std::is_floating_point_v<T>,
        "RoomReverb requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr RoomReverb() noexcept = default;

    /**
     * @brief Prepare the reverb for audio processing.
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
            for (auto& comb : channel.combs)
            {
                comb.delay.prepare(delaySampleRate);
                comb.damping.prepare(delaySampleRate);
            }

            for (auto& diffuser : channel.diffusers)
            {
                diffuser.prepare(
                    sampleRate_,
                    cvdsp::delay::DelayLine<T>::getMaxDelaySamples());
            }
        }

        updateDampingFilters();
        updateDelayTimes();
        updateFeedbackGains();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear internal delay and filter state.
     */
    void reset() noexcept
    {
        for (auto& channel : channels_)
        {
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

        updateDelayTimes();
    }

    /**
     * @brief Process an interleaving-free audio buffer view in place.
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
                const T input = channel[sample];
                const T wet = processTank(input, channels_[0]);
                channel[sample] = mixOutput(input, wet);
            }
            else
            {
                T* left = buffer.getChannel(0);
                T* right = buffer.getChannel(1);

                const T inputLeft = left[sample];
                const T inputRight = right[sample];

                const T monoDrive =
                    (inputLeft + inputRight)
                    * static_cast<T>(0.5);

                const T wetLeft =
                    processTank(
                        (monoDrive * static_cast<T>(0.75))
                        + (inputLeft * static_cast<T>(0.25)),
                        channels_[0]);

                const T wetRight =
                    processTank(
                        (monoDrive * static_cast<T>(0.75))
                        + (inputRight * static_cast<T>(0.25)),
                        channels_[1]);

                left[sample] = mixOutput(inputLeft, wetLeft);
                right[sample] = mixOutput(inputRight, wetRight);
            }
        }
    }

    /**
     * @brief Set virtual room size.
     * @param roomSize Normalized range 0..1. Larger values increase delay taps.
     */
    void setRoomSize(
        T roomSize) noexcept
    {
        roomSize_ =
            std::clamp(
                roomSize,
                static_cast<T>(0),
                static_cast<T>(1));

        updateDelayTimes();
        updateFeedbackGains();
    }

    /**
     * @brief Set RT60 decay time in seconds.
     * @param decayTimeSeconds Clamped to the short-room range 0.05..1.20 s.
     */
    void setDecayTime(
        T decayTimeSeconds) noexcept
    {
        decayTime_ =
            std::clamp(
                decayTimeSeconds,
                static_cast<T>(0.05),
                static_cast<T>(1.20));

        updateFeedbackGains();
    }

    /**
     * @brief Set high-frequency damping amount.
     * @param damping Normalized range 0..1. Larger values produce darker tails.
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
    static constexpr size_type kNumCombs = 4;
    static constexpr size_type kNumDiffusers = 2;
    static constexpr size_type kMaxCombDelaySamples = 8192;

    using CombDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxCombDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    struct CombFilter
    {
        CombDelay delay{};
        cvdsp::filters::LowPassOnePole<T> damping{};
        size_type delaySamples = 1;
        T feedback = static_cast<T>(0.1);
    };

    struct ChannelState
    {
        std::array<CombFilter, kNumCombs> combs{};
        std::array<cvdsp::filters::AllPassFilter<T>, kNumDiffusers> diffusers{};
    };

    static constexpr std::array<T, kNumCombs> kBaseCombMs{
        static_cast<T>(13.10),
        static_cast<T>(16.70),
        static_cast<T>(19.30),
        static_cast<T>(23.90)};

    static constexpr std::array<T, kNumDiffusers> kBaseAllPassMs{
        static_cast<T>(4.80),
        static_cast<T>(1.70)};

    [[nodiscard]]
    static T roomScale(
        T roomSize) noexcept
    {
        return
            static_cast<T>(0.55)
            + (roomSize * static_cast<T>(0.90));
    }

    [[nodiscard]]
    size_type millisecondsToSamples(
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
    size_type allPassMillisecondsToSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(2048)));
    }

    void updateDelayTimes() noexcept
    {
        const T scale = roomScale(roomSize_);

        for (size_type channel = 0; channel < kNumTanks; ++channel)
        {
            const T stereoOffsetMs =
                (channel == 0)
                    ? static_cast<T>(0)
                    : static_cast<T>(0.63);

            for (size_type index = 0; index < kNumCombs; ++index)
            {
                auto& comb = channels_[channel].combs[index];
                comb.delaySamples =
                    millisecondsToSamples(
                        (kBaseCombMs[index] * scale)
                        + stereoOffsetMs);
            }

            for (size_type index = 0; index < kNumDiffusers; ++index)
            {
                const size_type delaySamples =
                    allPassMillisecondsToSamples(
                        (kBaseAllPassMs[index] * scale)
                        + (stereoOffsetMs * static_cast<T>(0.25)));

                channels_[channel].diffusers[index].setDelaySamples(delaySamples);
            }
        }
    }

    void updateFeedbackGains() noexcept
    {
        const T safeDecay =
            std::max(
                decayTime_,
                static_cast<T>(0.05));

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
                            (static_cast<T>(-3) * delaySeconds) / safeDecay),
                        static_cast<T>(0.02),
                        static_cast<T>(0.84));
            }
        }
    }

    void updateDampingFilters() noexcept
    {
        const T minimumCutoff = static_cast<T>(1200);
        const T maximumCutoff = static_cast<T>(16000);
        const T cutoff =
            maximumCutoff
            + ((minimumCutoff - maximumCutoff) * damping_);

        for (auto& channel : channels_)
        {
            for (auto& comb : channel.combs)
            {
                comb.damping.setCutoffHz(cutoff);
            }
        }
    }

    [[nodiscard]]
    T processTank(
        T input,
        ChannelState& channel) noexcept
    {
        T combSum = static_cast<T>(0);

        for (auto& comb : channel.combs)
        {
            const T delayed =
                comb.delay.readIntegerSamples(comb.delaySamples);

            const T damped =
                comb.damping.process(delayed);

            comb.delay.write(
                input
                + (damped * comb.feedback));

            combSum += delayed;
        }

        T wet = combSum * static_cast<T>(0.25);

        for (auto& diffuser : channel.diffusers)
        {
            wet = diffuser.process(wet);
        }

        return wet;
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
    T roomSize_ = static_cast<T>(0.35);
    T decayTime_ = static_cast<T>(0.35);
    T damping_ = static_cast<T>(0.55);
    T wet_ = static_cast<T>(0.20);
    T dry_ = static_cast<T>(1);

    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_ROOMREVERB_HPP
