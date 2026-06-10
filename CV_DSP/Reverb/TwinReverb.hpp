#ifndef CVDSP_REVERB_TWINREVERB_HPP
#define CVDSP_REVERB_TWINREVERB_HPP

/**
 * @file TwinReverb.hpp
 * @brief Fender Twin Reverb-inspired clean triple-spring tank DSP.
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
 * @brief Clean Fender Twin Reverb-style spring tank model.
 *
 * Topology:
 *
 * guitar input -> fixed Fender-inspired tone shaping -> dwell gain
 *              -> 3 parallel virtual springs with mutually-prime delay times
 *              -> fixed/post tone shaping -> stereo wet projection -> mix
 *
 * The model intentionally avoids any input clipping or nonlinear drive so that
 * guitar pick transients remain intact. Dwell changes only the excitation level
 * of the virtual tank and the perceived boing intensity. All memory is fixed
 * inside cvdsp::delay::DelayLine<T> instances and the audio loop allocates
 * nothing.
 */
template<typename T>
class TwinReverbDSP
{
    static_assert(
        std::is_floating_point_v<T>,
        "TwinReverbDSP requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr TwinReverbDSP() noexcept = default;

    /**
     * @brief Prepare the Twin Reverb-style spring tank.
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
            static_cast<typename SpringDelay::size_type>(sampleRate_);

        for (auto& spring : springs_)
        {
            spring.delay.prepare(delaySampleRate);
            spring.damping.prepare(delaySampleRate);

            for (auto& diffuser : spring.diffusers)
            {
                diffuser.prepare(delaySampleRate);
            }
        }

        lowShelf_.prepare(sampleRate_);
        midScoop_.prepare(sampleRate_);
        highShelf_.prepare(sampleRate_);
        tankBandpass_.prepare(sampleRate_);
        for (auto& filter : outputSparkle_)
        {
            filter.prepare(sampleRate_);
        }

        updateToneShaping();
        updateSpringNetwork();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear all tank, diffuser, and filter state.
     */
    void reset() noexcept
    {
        lowShelf_.reset();
        midScoop_.reset();
        highShelf_.reset();
        tankBandpass_.reset();
        for (auto& filter : outputSparkle_)
        {
            filter.reset();
        }

        for (auto& spring : springs_)
        {
            spring.delay.reset();
            spring.damping.reset();

            for (auto& diffuser : spring.diffusers)
            {
                diffuser.reset();
            }
        }

        updateSpringNetwork();
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
     * @brief Set overall spring return level.
     * @param amount Range 0..1.
     */
    void setReverbAmount(
        T amount) noexcept
    {
        reverbAmount_ =
            std::clamp(
                amount,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Set tank drive/excitation without clipping the input.
     * @param dwell Range 0..1. Higher values increase boing intensity.
     */
    void setDwell(
        T dwell) noexcept
    {
        dwell_ =
            std::clamp(
                dwell,
                static_cast<T>(0),
                static_cast<T>(1));

        updateSpringNetwork();
    }

    /**
     * @brief Set Fender-style wet tone brightness.
     * @param tone Range 0..1. Higher values add more crystalline highs.
     */
    void setTone(
        T tone) noexcept
    {
        tone_ =
            std::clamp(
                tone,
                static_cast<T>(0),
                static_cast<T>(1));

        updateToneShaping();
        updateSpringNetwork();
    }

    /**
     * @brief Set final wet/dry mix.
     * @param mix Range 0..1.
     */
    void setMix(
        T mix) noexcept
    {
        mix_ =
            std::clamp(
                mix,
                static_cast<T>(0),
                static_cast<T>(1));
    }

private:

    static constexpr size_type kNumSprings = 3;
    static constexpr size_type kNumDiffusers = 3;
    static constexpr size_type kMaxSpringDelaySamples = 8192;
    static constexpr size_type kMaxDiffuserDelaySamples = 2048;

    using SpringDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxSpringDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    using DiffuserDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxDiffuserDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    struct DiffusionAllPass
    {
        DiffuserDelay delay{};
        size_type delaySamples = 1;
        T coefficient = static_cast<T>(0.52);

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

        void setCoefficient(
            T newCoefficient) noexcept
        {
            coefficient =
                std::clamp(
                    newCoefficient,
                    static_cast<T>(-0.90),
                    static_cast<T>(0.90));
        }

        [[nodiscard]]
        T process(
            T input) noexcept
        {
            const T delayed = delay.readIntegerSamples(delaySamples);
            const T output = delayed - (coefficient * input);

            delay.write(
                input + (coefficient * output));

            return output;
        }
    };

    struct SpringVoice
    {
        SpringDelay delay{};
        cvdsp::filters::LowPassOnePole<T> damping{};
        std::array<DiffusionAllPass, kNumDiffusers> diffusers{};
        size_type delaySamples = 1;
        T inputGain = static_cast<T>(1);
        T outputGainLeft = static_cast<T>(1);
        T outputGainRight = static_cast<T>(1);
    };

    static constexpr std::array<T, kNumSprings> kBaseSpringMs{
        static_cast<T>(31.0),
        static_cast<T>(37.0),
        static_cast<T>(41.0)};

    static constexpr std::array<T, kNumDiffusers> kBaseDiffuserMs{
        static_cast<T>(2.70),
        static_cast<T>(4.10),
        static_cast<T>(6.30)};

    [[nodiscard]]
    size_type millisecondsToSpringSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(kMaxSpringDelaySamples - 1)));
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

    void updateToneShaping() noexcept
    {
        const T sparkleGainDb =
            static_cast<T>(1.5)
            + (tone_ * static_cast<T>(4.5));

        const T scoopDepthDb =
            static_cast<T>(-5.5)
            - ((static_cast<T>(1) - tone_) * static_cast<T>(1.5));

        lowShelf_.setType(cvdsp::filters::BiquadType::LowShelf);
        lowShelf_.setFrequency(static_cast<T>(120));
        lowShelf_.setQ(static_cast<T>(0.7071067811865476));
        lowShelf_.setGainDB(static_cast<T>(2.2));
        lowShelf_.updateCoefficients();

        midScoop_.setType(cvdsp::filters::BiquadType::PeakingEQ);
        midScoop_.setFrequency(static_cast<T>(620));
        midScoop_.setQ(static_cast<T>(0.82));
        midScoop_.setGainDB(scoopDepthDb);
        midScoop_.updateCoefficients();

        highShelf_.setType(cvdsp::filters::BiquadType::HighShelf);
        highShelf_.setFrequency(static_cast<T>(3600));
        highShelf_.setQ(static_cast<T>(0.7071067811865476));
        highShelf_.setGainDB(sparkleGainDb);
        highShelf_.updateCoefficients();

        tankBandpass_.setType(cvdsp::filters::BiquadType::BandPass);
        tankBandpass_.setFrequency(
            static_cast<T>(1500) + (tone_ * static_cast<T>(520)));
        tankBandpass_.setQ(static_cast<T>(0.92));
        tankBandpass_.updateCoefficients();

        for (auto& filter : outputSparkle_)
        {
            filter.setType(cvdsp::filters::BiquadType::HighShelf);
            filter.setFrequency(
                static_cast<T>(3100) + (tone_ * static_cast<T>(900)));
            filter.setQ(static_cast<T>(0.7071067811865476));
            filter.setGainDB(
                static_cast<T>(0.5) + (tone_ * static_cast<T>(3.0)));
            filter.updateCoefficients();
        }
    }

    void updateSpringNetwork() noexcept
    {
        const T dwellDrive =
            static_cast<T>(0.55)
            + (dwell_ * static_cast<T>(1.65));

        feedbackGain_ =
            std::clamp(
                static_cast<T>(0.42) + (dwell_ * static_cast<T>(0.18)),
                static_cast<T>(0.35),
                static_cast<T>(0.64));

        const T dampingCutoff =
            static_cast<T>(3600)
            + (tone_ * static_cast<T>(5600));

        for (size_type springIndex = 0; springIndex < kNumSprings; ++springIndex)
        {
            auto& spring = springs_[springIndex];

            const T fineSpreadMs =
                static_cast<T>(springIndex) * static_cast<T>(0.17);

            spring.delaySamples =
                millisecondsToSpringSamples(
                    kBaseSpringMs[springIndex] + fineSpreadMs);

            spring.inputGain =
                dwellDrive
                * (static_cast<T>(0.74)
                   + (static_cast<T>(springIndex) * static_cast<T>(0.08)));

            spring.outputGainLeft =
                kOutputLeft[springIndex];

            spring.outputGainRight =
                kOutputRight[springIndex];

            spring.damping.setCutoffHz(dampingCutoff);

            for (size_type diffuserIndex = 0; diffuserIndex < kNumDiffusers; ++diffuserIndex)
            {
                const T delayMs =
                    kBaseDiffuserMs[diffuserIndex]
                    + (static_cast<T>(springIndex) * static_cast<T>(0.31))
                    + (dwell_ * static_cast<T>(0.18));

                spring.diffusers[diffuserIndex].setDelaySamples(
                    millisecondsToDiffuserSamples(delayMs));

                const T sign =
                    ((springIndex + diffuserIndex) % 2u == 0u)
                        ? static_cast<T>(1)
                        : static_cast<T>(-1);

                spring.diffusers[diffuserIndex].setCoefficient(
                    sign
                    * (static_cast<T>(0.45)
                       + (static_cast<T>(diffuserIndex) * static_cast<T>(0.055))));
            }
        }
    }

    [[nodiscard]]
    T processSpring(
        T excitation,
        SpringVoice& spring) noexcept
    {
        const T delayed =
            spring.delay.readIntegerSamples(spring.delaySamples);

        const T damped = spring.damping.process(delayed);

        T writeSample =
            (excitation * spring.inputGain)
            + (damped * feedbackGain_);

        for (auto& diffuser : spring.diffusers)
        {
            writeSample = diffuser.process(writeSample);
        }

        spring.delay.write(writeSample);

        return delayed;
    }

    void processSample(
        T input,
        T& wetLeft,
        T& wetRight) noexcept
    {
        T shaped = lowShelf_.process(input);
        shaped = midScoop_.process(shaped);
        shaped = highShelf_.process(shaped);

        const T excitation = tankBandpass_.process(shaped);

        const T spring0 = processSpring(excitation, springs_[0]);
        const T spring1 = processSpring(excitation, springs_[1]);
        const T spring2 = processSpring(excitation, springs_[2]);

        const T tankLeft =
            (spring0 * springs_[0].outputGainLeft)
            + (spring1 * springs_[1].outputGainLeft)
            + (spring2 * springs_[2].outputGainLeft);

        const T tankRight =
            (spring0 * springs_[0].outputGainRight)
            + (spring1 * springs_[1].outputGainRight)
            + (spring2 * springs_[2].outputGainRight);

        wetLeft = outputSparkle_[0].process(tankLeft * reverbAmount_ * static_cast<T>(0.44));
        wetRight = outputSparkle_[1].process(tankRight * reverbAmount_ * static_cast<T>(0.44));
    }

    [[nodiscard]]
    T mixOutput(
        T dryInput,
        T wetInput) const noexcept
    {
        return
            (dryInput * (static_cast<T>(1) - mix_))
            + (wetInput * mix_);
    }

    static constexpr std::array<T, kNumSprings> kOutputLeft{
        static_cast<T>(0.92),
        static_cast<T>(-0.54),
        static_cast<T>(0.68)};

    static constexpr std::array<T, kNumSprings> kOutputRight{
        static_cast<T>(0.58),
        static_cast<T>(0.84),
        static_cast<T>(-0.48)};

    std::array<SpringVoice, kNumSprings> springs_{};

    cvdsp::filters::Biquad<T> lowShelf_{};
    cvdsp::filters::Biquad<T> midScoop_{};
    cvdsp::filters::Biquad<T> highShelf_{};
    cvdsp::filters::Biquad<T> tankBandpass_{};
    std::array<cvdsp::filters::Biquad<T>, 2> outputSparkle_{};

    T sampleRate_ = static_cast<T>(44100);
    T reverbAmount_ = static_cast<T>(0.60);
    T dwell_ = static_cast<T>(0.45);
    T tone_ = static_cast<T>(0.62);
    T mix_ = static_cast<T>(0.24);
    T feedbackGain_ = static_cast<T>(0.50);

    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_TWINREVERB_HPP
