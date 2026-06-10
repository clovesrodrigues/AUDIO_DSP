#ifndef CVDSP_REVERB_SUPERREVERB_HPP
#define CVDSP_REVERB_SUPERREVERB_HPP

/**
 * @file SuperReverb.hpp
 * @brief Fender Super Reverb 4x10-inspired spring reverb DSP.
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
 * @brief Fender Super Reverb-style spring tank with 4x10 cabinet diffusion.
 *
 * Topology:
 *
 * guitar input -> aggressive vintage Super tone voicing -> dynamic dwell drive
 *              -> three parallel virtual spring paths
 *              -> four-node cross-channel cabinet diffusion network
 *              -> stereo air/presence return shaping -> wet/dry mix
 *
 * The spring section supplies the metallic tank splash while the cabinet
 * diffusion network emulates the wider, airy acoustic spread of a 4x10 combo.
 * The dry path stays linear and unclipped, keeping electric-blues pick attack
 * intact while the wet tail expands around it.
 */
template<typename T>
class SuperReverbDSP
{
    static_assert(
        std::is_floating_point_v<T>,
        "SuperReverbDSP requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr SuperReverbDSP() noexcept = default;

    /**
     * @brief Prepare the Super Reverb-style processor.
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

        const auto springSampleRate =
            static_cast<typename SpringDelay::size_type>(sampleRate_);

        for (auto& spring : springs_)
        {
            spring.delay.prepare(springSampleRate);
            spring.damping.prepare(springSampleRate);

            for (auto& diffuser : spring.diffusers)
            {
                diffuser.prepare(springSampleRate);
            }
        }

        for (auto& node : cabinetNodes_)
        {
            node.delay.prepare(springSampleRate);
        }

        inputLowShelf_.prepare(sampleRate_);
        presencePeak_.prepare(sampleRate_);
        biteShelf_.prepare(sampleRate_);
        tankBandpass_.prepare(sampleRate_);

        for (auto& filter : returnAir_)
        {
            filter.prepare(sampleRate_);
        }

        updateEnvelopeCoefficients();
        updateToneShaping();
        updateSpringNetwork();
        updateCabinetDiffusion();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear spring, cabinet diffusion, filter, and envelope state.
     */
    void reset() noexcept
    {
        inputLowShelf_.reset();
        presencePeak_.reset();
        biteShelf_.reset();
        tankBandpass_.reset();

        for (auto& filter : returnAir_)
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

        for (auto& node : cabinetNodes_)
        {
            node.delay.reset();
        }

        envelope_ = static_cast<T>(0);
        updateSpringNetwork();
        updateCabinetDiffusion();
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
     * @brief Set spring return level.
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
     * @brief Set clean spring-tank excitation.
     * @param dwell Range 0..1. Higher values produce stronger spring splash.
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
        updateCabinetDiffusion();
    }

    /**
     * @brief Set aggressive vintage Super voicing.
     * @param tone Range 0..1. Higher values emphasize bite and 4x10 air.
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
     * @brief Set stereo expansion of the decorrelated reverb tails.
     * @param width Range 0..1.
     */
    void setWidth(
        T width) noexcept
    {
        width_ =
            std::clamp(
                width,
                static_cast<T>(0),
                static_cast<T>(1));

        updateCabinetDiffusion();
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
    static constexpr size_type kNumSpringDiffusers = 3;
    static constexpr size_type kNumCabinetNodes = 4;
    static constexpr size_type kMaxSpringDelaySamples = 8192;
    static constexpr size_type kMaxDiffuserDelaySamples = 2048;
    static constexpr size_type kMaxCabinetDelaySamples = 2048;

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

    using CabinetDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxCabinetDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    struct DiffusionAllPass
    {
        DiffuserDelay delay{};
        size_type delaySamples = 1;
        T coefficient = static_cast<T>(0.50);

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
        std::array<DiffusionAllPass, kNumSpringDiffusers> diffusers{};
        size_type delaySamples = 1;
        T inputGain = static_cast<T>(1);
        T outputGainLeft = static_cast<T>(1);
        T outputGainRight = static_cast<T>(1);
    };

    struct CabinetNode
    {
        CabinetDelay delay{};
        size_type delaySamples = 1;
    };

    static constexpr std::array<T, kNumSprings> kBaseSpringMs{
        static_cast<T>(30.50),
        static_cast<T>(36.70),
        static_cast<T>(42.10)};

    static constexpr std::array<T, kNumSpringDiffusers> kBaseDiffuserMs{
        static_cast<T>(2.50),
        static_cast<T>(4.30),
        static_cast<T>(6.70)};

    static constexpr std::array<T, kNumCabinetNodes> kBaseCabinetMs{
        static_cast<T>(2.90),
        static_cast<T>(4.70),
        static_cast<T>(6.10),
        static_cast<T>(8.30)};

    static constexpr std::array<T, kNumSprings> kOutputLeft{
        static_cast<T>(0.94),
        static_cast<T>(-0.58),
        static_cast<T>(0.62)};

    static constexpr std::array<T, kNumSprings> kOutputRight{
        static_cast<T>(0.54),
        static_cast<T>(0.90),
        static_cast<T>(-0.52)};

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

    [[nodiscard]]
    size_type millisecondsToCabinetSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(kMaxCabinetDelaySamples - 1)));
    }

    void updateEnvelopeCoefficients() noexcept
    {
        const T attackMs = static_cast<T>(1.8);
        const T releaseMs = static_cast<T>(120.0);

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
        const T presenceDb =
            static_cast<T>(2.4)
            + (tone_ * static_cast<T>(3.2));

        const T biteDb =
            static_cast<T>(1.2)
            + (tone_ * static_cast<T>(3.8));

        inputLowShelf_.setType(cvdsp::filters::BiquadType::LowShelf);
        inputLowShelf_.setFrequency(static_cast<T>(115));
        inputLowShelf_.setQ(static_cast<T>(0.7071067811865476));
        inputLowShelf_.setGainDB(static_cast<T>(1.7));
        inputLowShelf_.updateCoefficients();

        presencePeak_.setType(cvdsp::filters::BiquadType::PeakingEQ);
        presencePeak_.setFrequency(
            static_cast<T>(930) + (tone_ * static_cast<T>(260)));
        presencePeak_.setQ(static_cast<T>(0.88));
        presencePeak_.setGainDB(presenceDb);
        presencePeak_.updateCoefficients();

        biteShelf_.setType(cvdsp::filters::BiquadType::HighShelf);
        biteShelf_.setFrequency(
            static_cast<T>(2850) + (tone_ * static_cast<T>(780)));
        biteShelf_.setQ(static_cast<T>(0.7071067811865476));
        biteShelf_.setGainDB(biteDb);
        biteShelf_.updateCoefficients();

        tankBandpass_.setType(cvdsp::filters::BiquadType::BandPass);
        tankBandpass_.setFrequency(
            static_cast<T>(1320) + (tone_ * static_cast<T>(520)));
        tankBandpass_.setQ(static_cast<T>(0.96));
        tankBandpass_.updateCoefficients();

        for (auto& filter : returnAir_)
        {
            filter.setType(cvdsp::filters::BiquadType::HighShelf);
            filter.setFrequency(
                static_cast<T>(3400) + (tone_ * static_cast<T>(900)));
            filter.setQ(static_cast<T>(0.7071067811865476));
            filter.setGainDB(
                static_cast<T>(0.8) + (tone_ * static_cast<T>(3.0)));
            filter.updateCoefficients();
        }
    }

    void updateSpringNetwork() noexcept
    {
        const T dwellDrive =
            static_cast<T>(0.58)
            + (dwell_ * static_cast<T>(1.72));

        feedbackGain_ =
            std::clamp(
                static_cast<T>(0.40) + (dwell_ * static_cast<T>(0.20)),
                static_cast<T>(0.34),
                static_cast<T>(0.64));

        const T dampingCutoff =
            static_cast<T>(3400)
            + (tone_ * static_cast<T>(5400));

        for (size_type springIndex = 0; springIndex < kNumSprings; ++springIndex)
        {
            auto& spring = springs_[springIndex];

            const T fineSpreadMs =
                static_cast<T>(springIndex) * static_cast<T>(0.19);

            spring.delaySamples =
                millisecondsToSpringSamples(
                    kBaseSpringMs[springIndex] + fineSpreadMs);

            spring.inputGain =
                dwellDrive
                * (static_cast<T>(0.76)
                   + (static_cast<T>(springIndex) * static_cast<T>(0.085)));

            spring.outputGainLeft = kOutputLeft[springIndex];
            spring.outputGainRight = kOutputRight[springIndex];
            spring.damping.setCutoffHz(dampingCutoff);

            for (size_type diffuserIndex = 0; diffuserIndex < kNumSpringDiffusers; ++diffuserIndex)
            {
                const T delayMs =
                    kBaseDiffuserMs[diffuserIndex]
                    + (static_cast<T>(springIndex) * static_cast<T>(0.33))
                    + (dwell_ * static_cast<T>(0.16));

                spring.diffusers[diffuserIndex].setDelaySamples(
                    millisecondsToDiffuserSamples(delayMs));

                const T sign =
                    ((springIndex + diffuserIndex) % 2u == 0u)
                        ? static_cast<T>(1)
                        : static_cast<T>(-1);

                spring.diffusers[diffuserIndex].setCoefficient(
                    sign
                    * (static_cast<T>(0.44)
                       + (static_cast<T>(diffuserIndex) * static_cast<T>(0.058))));
            }
        }
    }

    void updateCabinetDiffusion() noexcept
    {
        for (size_type nodeIndex = 0; nodeIndex < kNumCabinetNodes; ++nodeIndex)
        {
            const T spreadMs =
                width_ * static_cast<T>(0.42)
                + dwell_ * static_cast<T>(0.08);

            cabinetNodes_[nodeIndex].delaySamples =
                millisecondsToCabinetSamples(
                    kBaseCabinetMs[nodeIndex]
                    + (static_cast<T>(nodeIndex) * spreadMs));
        }

        cabinetFeedback_ =
            static_cast<T>(0.18)
            + (width_ * static_cast<T>(0.34));
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
            envelope_ * static_cast<T>(3.8),
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

    void processCabinetDiffusion(
        T tankLeft,
        T tankRight,
        T& wetLeft,
        T& wetRight) noexcept
    {
        const T delayed0 = cabinetNodes_[0].delay.readIntegerSamples(cabinetNodes_[0].delaySamples);
        const T delayed1 = cabinetNodes_[1].delay.readIntegerSamples(cabinetNodes_[1].delaySamples);
        const T delayed2 = cabinetNodes_[2].delay.readIntegerSamples(cabinetNodes_[2].delaySamples);
        const T delayed3 = cabinetNodes_[3].delay.readIntegerSamples(cabinetNodes_[3].delaySamples);

        constexpr T kMatrixScale = static_cast<T>(0.5);
        constexpr T kInputGain = static_cast<T>(0.58);

        const T feed0 = (delayed0 + delayed1 + delayed2 + delayed3) * kMatrixScale;
        const T feed1 = (delayed0 - delayed1 + delayed2 - delayed3) * kMatrixScale;
        const T feed2 = (delayed0 + delayed1 - delayed2 - delayed3) * kMatrixScale;
        const T feed3 = (delayed0 - delayed1 - delayed2 + delayed3) * kMatrixScale;

        cabinetNodes_[0].delay.write(
            ((tankLeft + (tankRight * static_cast<T>(0.18))) * kInputGain)
            + (feed0 * cabinetFeedback_));

        cabinetNodes_[1].delay.write(
            ((tankRight + (tankLeft * static_cast<T>(0.18))) * kInputGain)
            + (feed1 * cabinetFeedback_));

        cabinetNodes_[2].delay.write(
            (((tankLeft + tankRight) * static_cast<T>(0.46)) * kInputGain)
            + (feed2 * cabinetFeedback_));

        cabinetNodes_[3].delay.write(
            (((tankLeft - tankRight) * static_cast<T>(0.38)) * kInputGain)
            + (feed3 * cabinetFeedback_));

        const T diffusedLeft =
            (tankLeft * static_cast<T>(0.62))
            + (delayed0 * static_cast<T>(0.58))
            - (delayed2 * static_cast<T>(0.32))
            + (delayed3 * static_cast<T>(0.22));

        const T diffusedRight =
            (tankRight * static_cast<T>(0.62))
            + (delayed1 * static_cast<T>(0.58))
            - (delayed2 * static_cast<T>(0.24))
            - (delayed3 * static_cast<T>(0.22));

        const T mid =
            (diffusedLeft + diffusedRight)
            * static_cast<T>(0.5);

        const T side =
            (diffusedLeft - diffusedRight)
            * static_cast<T>(0.5);

        const T sideGain =
            static_cast<T>(0.40)
            + (width_ * static_cast<T>(1.35));

        wetLeft = mid + (side * sideGain);
        wetRight = mid - (side * sideGain);
    }

    void processSample(
        T input,
        T& wetLeft,
        T& wetRight) noexcept
    {
        T shaped = inputLowShelf_.process(input);
        shaped = presencePeak_.process(shaped);
        shaped = biteShelf_.process(shaped);

        const T attackAmount = updateEnvelope(shaped);
        const T dynamicDwell =
            static_cast<T>(0.86)
            + (attackAmount * static_cast<T>(0.30));

        const T excitation =
            tankBandpass_.process(shaped)
            * dynamicDwell;

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

        T diffusedLeft = static_cast<T>(0);
        T diffusedRight = static_cast<T>(0);
        processCabinetDiffusion(tankLeft, tankRight, diffusedLeft, diffusedRight);

        wetLeft =
            returnAir_[0].process(
                diffusedLeft * reverbAmount_ * static_cast<T>(0.42));

        wetRight =
            returnAir_[1].process(
                diffusedRight * reverbAmount_ * static_cast<T>(0.42));
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
    std::array<CabinetNode, kNumCabinetNodes> cabinetNodes_{};

    cvdsp::filters::Biquad<T> inputLowShelf_{};
    cvdsp::filters::Biquad<T> presencePeak_{};
    cvdsp::filters::Biquad<T> biteShelf_{};
    cvdsp::filters::Biquad<T> tankBandpass_{};
    std::array<cvdsp::filters::Biquad<T>, 2> returnAir_{};

    T sampleRate_ = static_cast<T>(44100);
    T reverbAmount_ = static_cast<T>(0.62);
    T dwell_ = static_cast<T>(0.48);
    T tone_ = static_cast<T>(0.66);
    T width_ = static_cast<T>(0.72);
    T mix_ = static_cast<T>(0.24);
    T feedbackGain_ = static_cast<T>(0.50);
    T cabinetFeedback_ = static_cast<T>(0.40);
    T envelope_ = static_cast<T>(0);
    T envelopeAttack_ = static_cast<T>(0.99);
    T envelopeRelease_ = static_cast<T>(0.999);

    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_SUPERREVERB_HPP
