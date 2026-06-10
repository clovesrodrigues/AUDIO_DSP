#ifndef CVDSP_REVERB_DELUXEREVERB_HPP
#define CVDSP_REVERB_DELUXEREVERB_HPP

/**
 * @file DeluxeReverb.hpp
 * @brief Fender Deluxe Reverb-inspired dual spring tank DSP.
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
 * @brief Fender Deluxe Reverb-style dual spring tank model.
 *
 * Topology:
 *
 * guitar input -> Deluxe-style tone voicing -> dynamic dwell excitation
 *              -> 2 parallel virtual springs with short dispersive all-pass chains
 *              -> sweet treble return shaping -> stereo wet projection -> mix
 *
 * Compared with a Twin-style spring, this model uses a smaller two-spring tank,
 * slightly shorter spring times, more present mids, and softer high-frequency
 * return shaping. The dry path is never clipped or saturated, so pick transients
 * remain intact while the spring tank reacts dynamically to attack intensity.
 */
template<typename T>
class DeluxeReverbDSP
{
    static_assert(
        std::is_floating_point_v<T>,
        "DeluxeReverbDSP requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr DeluxeReverbDSP() noexcept = default;

    /**
     * @brief Prepare the Deluxe Reverb-style spring tank.
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

        inputLowShelf_.prepare(sampleRate_);
        inputMidVoice_.prepare(sampleRate_);
        inputSweetTreble_.prepare(sampleRate_);
        tankBandpass_.prepare(sampleRate_);

        for (auto& filter : returnSweetener_)
        {
            filter.prepare(sampleRate_);
        }

        updateEnvelopeCoefficients();
        updateToneShaping();
        updateSpringNetwork();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear all spring, diffuser, filter, and envelope state.
     */
    void reset() noexcept
    {
        inputLowShelf_.reset();
        inputMidVoice_.reset();
        inputSweetTreble_.reset();
        tankBandpass_.reset();

        for (auto& filter : returnSweetener_)
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

        envelope_ = static_cast<T>(0);
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
     * @brief Set clean tank excitation gain.
     * @param dwell Range 0..1. Higher values produce stronger spring splash/boing.
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
     * @brief Set Deluxe-style return tone.
     * @param tone Range 0..1. Higher values add sweet treble without Twin-like size.
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

    static constexpr size_type kNumSprings = 2;
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
        T coefficient = static_cast<T>(0.48);

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
                    static_cast<T>(-0.88),
                    static_cast<T>(0.88));
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
        static_cast<T>(29.0),
        static_cast<T>(34.0)};

    static constexpr std::array<T, kNumDiffusers> kBaseDiffuserMs{
        static_cast<T>(2.30),
        static_cast<T>(3.80),
        static_cast<T>(5.90)};

    static constexpr std::array<T, kNumSprings> kOutputLeft{
        static_cast<T>(0.88),
        static_cast<T>(-0.52)};

    static constexpr std::array<T, kNumSprings> kOutputRight{
        static_cast<T>(0.56),
        static_cast<T>(0.82)};

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

    void updateEnvelopeCoefficients() noexcept
    {
        const T attackMs = static_cast<T>(2.5);
        const T releaseMs = static_cast<T>(95.0);

        envelopeAttack_ =
            std::exp(
                static_cast<T>(-1)
                / ((attackMs * static_cast<T>(0.001)) * sampleRate_));

        envelopeRelease_ =
            std::exp(
                static_cast<T>(-1)
                / ((releaseMs * static_cast<T>(0.001)) * sampleRate_));
    }

    void updateToneShaping() noexcept
    {
        const T midGainDb =
            static_cast<T>(0.8)
            + (tone_ * static_cast<T>(1.8));

        const T sweetTrebleDb =
            static_cast<T>(0.4)
            + (tone_ * static_cast<T>(2.6));

        inputLowShelf_.setType(cvdsp::filters::BiquadType::LowShelf);
        inputLowShelf_.setFrequency(static_cast<T>(135));
        inputLowShelf_.setQ(static_cast<T>(0.7071067811865476));
        inputLowShelf_.setGainDB(static_cast<T>(1.2));
        inputLowShelf_.updateCoefficients();

        inputMidVoice_.setType(cvdsp::filters::BiquadType::PeakingEQ);
        inputMidVoice_.setFrequency(static_cast<T>(720));
        inputMidVoice_.setQ(static_cast<T>(0.92));
        inputMidVoice_.setGainDB(midGainDb);
        inputMidVoice_.updateCoefficients();

        inputSweetTreble_.setType(cvdsp::filters::BiquadType::HighShelf);
        inputSweetTreble_.setFrequency(static_cast<T>(3300));
        inputSweetTreble_.setQ(static_cast<T>(0.7071067811865476));
        inputSweetTreble_.setGainDB(sweetTrebleDb);
        inputSweetTreble_.updateCoefficients();

        tankBandpass_.setType(cvdsp::filters::BiquadType::BandPass);
        tankBandpass_.setFrequency(
            static_cast<T>(1180) + (tone_ * static_cast<T>(420)));
        tankBandpass_.setQ(static_cast<T>(0.82));
        tankBandpass_.updateCoefficients();

        for (auto& filter : returnSweetener_)
        {
            filter.setType(cvdsp::filters::BiquadType::HighShelf);
            filter.setFrequency(
                static_cast<T>(2850) + (tone_ * static_cast<T>(650)));
            filter.setQ(static_cast<T>(0.7071067811865476));
            filter.setGainDB(
                static_cast<T>(0.2) + (tone_ * static_cast<T>(1.8)));
            filter.updateCoefficients();
        }
    }

    void updateSpringNetwork() noexcept
    {
        const T dwellDrive =
            static_cast<T>(0.48)
            + (dwell_ * static_cast<T>(1.42));

        feedbackGain_ =
            std::clamp(
                static_cast<T>(0.38) + (dwell_ * static_cast<T>(0.17)),
                static_cast<T>(0.30),
                static_cast<T>(0.58));

        const T dampingCutoff =
            static_cast<T>(3000)
            + (tone_ * static_cast<T>(4300));

        for (size_type springIndex = 0; springIndex < kNumSprings; ++springIndex)
        {
            auto& spring = springs_[springIndex];

            const T fineSpreadMs =
                static_cast<T>(springIndex) * static_cast<T>(0.23);

            spring.delaySamples =
                millisecondsToSpringSamples(
                    kBaseSpringMs[springIndex] + fineSpreadMs);

            spring.inputGain =
                dwellDrive
                * (static_cast<T>(0.82)
                   + (static_cast<T>(springIndex) * static_cast<T>(0.10)));

            spring.outputGainLeft = kOutputLeft[springIndex];
            spring.outputGainRight = kOutputRight[springIndex];
            spring.damping.setCutoffHz(dampingCutoff);

            for (size_type diffuserIndex = 0; diffuserIndex < kNumDiffusers; ++diffuserIndex)
            {
                const T delayMs =
                    kBaseDiffuserMs[diffuserIndex]
                    + (static_cast<T>(springIndex) * static_cast<T>(0.29))
                    + (dwell_ * static_cast<T>(0.12));

                spring.diffusers[diffuserIndex].setDelaySamples(
                    millisecondsToDiffuserSamples(delayMs));

                const T sign =
                    ((springIndex + diffuserIndex) % 2u == 0u)
                        ? static_cast<T>(1)
                        : static_cast<T>(-1);

                spring.diffusers[diffuserIndex].setCoefficient(
                    sign
                    * (static_cast<T>(0.42)
                       + (static_cast<T>(diffuserIndex) * static_cast<T>(0.052))));
            }
        }
    }

    [[nodiscard]]
    T updateEnvelope(
        T input) noexcept
    {
        const T magnitude = std::abs(input);
        const T coefficient =
            (magnitude > envelope_)
                ? envelopeAttack_
                : envelopeRelease_;

        envelope_ =
            (coefficient * envelope_)
            + ((static_cast<T>(1) - coefficient) * magnitude);

        return std::clamp(
            envelope_ * static_cast<T>(3.5),
            static_cast<T>(0),
            static_cast<T>(1));
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
        T shaped = inputLowShelf_.process(input);
        shaped = inputMidVoice_.process(shaped);
        shaped = inputSweetTreble_.process(shaped);

        const T attackAmount = updateEnvelope(shaped);
        const T dynamicDwell =
            static_cast<T>(0.82)
            + (attackAmount * static_cast<T>(0.33));

        const T excitation =
            tankBandpass_.process(shaped)
            * dynamicDwell;

        const T spring0 = processSpring(excitation, springs_[0]);
        const T spring1 = processSpring(excitation, springs_[1]);

        const T tankLeft =
            (spring0 * springs_[0].outputGainLeft)
            + (spring1 * springs_[1].outputGainLeft);

        const T tankRight =
            (spring0 * springs_[0].outputGainRight)
            + (spring1 * springs_[1].outputGainRight);

        wetLeft =
            returnSweetener_[0].process(
                tankLeft * reverbAmount_ * static_cast<T>(0.52));

        wetRight =
            returnSweetener_[1].process(
                tankRight * reverbAmount_ * static_cast<T>(0.52));
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

    std::array<SpringVoice, kNumSprings> springs_{};

    cvdsp::filters::Biquad<T> inputLowShelf_{};
    cvdsp::filters::Biquad<T> inputMidVoice_{};
    cvdsp::filters::Biquad<T> inputSweetTreble_{};
    cvdsp::filters::Biquad<T> tankBandpass_{};
    std::array<cvdsp::filters::Biquad<T>, 2> returnSweetener_{};

    T sampleRate_ = static_cast<T>(44100);
    T reverbAmount_ = static_cast<T>(0.55);
    T dwell_ = static_cast<T>(0.42);
    T tone_ = static_cast<T>(0.56);
    T mix_ = static_cast<T>(0.22);
    T feedbackGain_ = static_cast<T>(0.45);
    T envelope_ = static_cast<T>(0);
    T envelopeAttack_ = static_cast<T>(0.99);
    T envelopeRelease_ = static_cast<T>(0.999);

    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_DELUXEREVERB_HPP
