#ifndef CVDSP_REVERB_SPRINGREVERB_HPP
#define CVDSP_REVERB_SPRINGREVERB_HPP

/**
 * @file SpringReverb.hpp
 * @brief Guitar-amplifier spring reverb based on dispersive multi-delay lines.
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
 * @brief Vintage guitar-amplifier spring reverb.
 *
 * Topology:
 *
 * input -> narrow band/tilt tone shaping -> three non-uniform spring delay paths
 *       -> per-path dispersive all-pass chain -> feedback/damping -> wet mixer
 *
 * The design intentionally emphasizes a narrow mid-band, non-uniform metallic
 * resonances, and short chirped dispersion bursts rather than a smooth room
 * tail. All buffers are fixed-size cvdsp::delay::DelayLine<T> instances and the
 * audio loop performs no heap allocation or resizing.
 */
template<typename T>
class SpringReverb
{
    static_assert(
        std::is_floating_point_v<T>,
        "SpringReverb requires floating point type");

public:

    using value_type = T;
    using size_type = std::size_t;

    constexpr SpringReverb() noexcept = default;

    /**
     * @brief Prepare the spring tank for processing.
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

        for (auto& path : paths_)
        {
            path.delay.prepare(delaySampleRate);
            path.damping.prepare(delaySampleRate);

            for (auto& disperser : path.dispersers)
            {
                disperser.prepare(delaySampleRate);
            }
        }

        inputBandpass_.prepare(sampleRate_);
        inputBandpass_.setType(cvdsp::filters::BiquadType::BandPass);
        inputBandpass_.setQ(static_cast<T>(1.15));

        outputTone_.prepare(sampleRate_);
        outputTone_.setType(cvdsp::filters::BiquadType::BandPass);
        outputTone_.setQ(static_cast<T>(0.95));

        updateToneFilters();
        updateDelayTimes();
        updateFeedbackGain();
        reset();

        prepared_ = true;
    }

    /**
     * @brief Clear delay and filter states.
     */
    void reset() noexcept
    {
        inputBandpass_.reset();
        outputTone_.reset();

        for (auto& path : paths_)
        {
            path.delay.reset();
            path.damping.reset();

            for (auto& disperser : path.dispersers)
            {
                disperser.reset();
            }
        }

        updateDelayTimes();
    }

    /**
     * @brief Process a non-interleaved buffer in place.
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
                const T wet = processSample(dryInput);
                channel[sample] = mixOutput(dryInput, wet);
            }
            else
            {
                T* left = buffer.getChannel(0);
                T* right = buffer.getChannel(1);

                const T dryLeft = left[sample];
                const T dryRight = right[sample];
                const T monoDrive =
                    (dryLeft + dryRight)
                    * static_cast<T>(0.5);

                const T wet = processSample(monoDrive);
                const T wetLeft = wet;
                const T wetRight = wet * static_cast<T>(0.92);

                left[sample] = mixOutput(dryLeft, wetLeft);
                right[sample] = mixOutput(dryRight, wetRight);
            }
        }
    }

    /**
     * @brief Set physical spring length.
     * @param springLength Range 0..1. Larger values lengthen the tank delays.
     */
    void setSpringLength(
        T springLength) noexcept
    {
        springLength_ =
            std::clamp(
                springLength,
                static_cast<T>(0),
                static_cast<T>(1));

        updateDelayTimes();
        updateFeedbackGain();
    }

    /**
     * @brief Set spring tension.
     * @param tension Range 0..1. Higher values tighten delays and chirp spacing.
     */
    void setTension(
        T tension) noexcept
    {
        tension_ =
            std::clamp(
                tension,
                static_cast<T>(0),
                static_cast<T>(1));

        updateDelayTimes();
        updateFeedbackGain();
    }

    /**
     * @brief Set tank tone.
     * @param tone Range 0..1. Low values are darker; high values are brighter.
     */
    void setTone(
        T tone) noexcept
    {
        tone_ =
            std::clamp(
                tone,
                static_cast<T>(0),
                static_cast<T>(1));

        updateToneFilters();
    }

    /**
     * @brief Set wet/dry mix.
     * @param mix Range 0..1. 0 is dry only, 1 is spring only.
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

    static constexpr size_type kNumPaths = 3;
    static constexpr size_type kNumDispersers = 4;
    static constexpr size_type kMaxSpringDelaySamples = 8192;
    static constexpr size_type kMaxDispersionDelaySamples = 2048;

    using SpringDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxSpringDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    using DispersionDelay =
        cvdsp::delay::DelayLine<
            T,
            kMaxDispersionDelaySamples,
            cvdsp::delay::InterpolationType::None>;

    struct DispersionAllPass
    {
        DispersionDelay delay{};
        size_type delaySamples = 1;
        T feedback = static_cast<T>(0.58);

        void prepare(
            typename DispersionDelay::size_type sampleRate) noexcept
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
                    kMaxDispersionDelaySamples - 1u);
        }

        void setFeedback(
            T newFeedback) noexcept
        {
            feedback =
                std::clamp(
                    newFeedback,
                    static_cast<T>(-0.92),
                    static_cast<T>(0.92));
        }

        [[nodiscard]]
        T process(
            T input) noexcept
        {
            const T delayed = delay.readIntegerSamples(delaySamples);
            const T output = delayed - (feedback * input);

            delay.write(
                input + (feedback * output));

            return output;
        }
    };

    struct SpringPath
    {
        SpringDelay delay{};
        cvdsp::filters::LowPassOnePole<T> damping{};
        std::array<DispersionAllPass, kNumDispersers> dispersers{};
        size_type delaySamples = 1;
        T inputGain = static_cast<T>(1);
        T outputGain = static_cast<T>(1);
    };

    static constexpr std::array<T, kNumPaths> kBaseDelayMs{
        static_cast<T>(23.70),
        static_cast<T>(31.10),
        static_cast<T>(41.90)};

    static constexpr std::array<T, kNumDispersers> kBaseDispersionMs{
        static_cast<T>(2.10),
        static_cast<T>(3.70),
        static_cast<T>(5.30),
        static_cast<T>(7.90)};

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
    size_type millisecondsToDispersionSamples(
        T milliseconds) const noexcept
    {
        const T samples =
            (milliseconds * sampleRate_)
            / static_cast<T>(1000);

        return static_cast<size_type>(
            std::clamp(
                samples,
                static_cast<T>(1),
                static_cast<T>(kMaxDispersionDelaySamples - 1)));
    }

    void updateDelayTimes() noexcept
    {
        const T lengthScale =
            static_cast<T>(0.72)
            + (springLength_ * static_cast<T>(0.86));

        const T tensionScale =
            static_cast<T>(1.18)
            - (tension_ * static_cast<T>(0.36));

        const T dispersionScale =
            static_cast<T>(1.35)
            - (tension_ * static_cast<T>(0.62));

        for (size_type pathIndex = 0; pathIndex < kNumPaths; ++pathIndex)
        {
            auto& path = paths_[pathIndex];

            const T pathSpread =
                static_cast<T>(pathIndex) * static_cast<T>(0.83);

            path.delaySamples =
                millisecondsToSpringSamples(
                    (kBaseDelayMs[pathIndex] * lengthScale * tensionScale)
                    + pathSpread);

            path.inputGain =
                static_cast<T>(0.78)
                + (static_cast<T>(pathIndex) * static_cast<T>(0.11));

            path.outputGain =
                (pathIndex == 1u)
                    ? static_cast<T>(-0.74)
                    : static_cast<T>(0.86)
                      - (static_cast<T>(pathIndex) * static_cast<T>(0.12));

            for (size_type diffuserIndex = 0; diffuserIndex < kNumDispersers; ++diffuserIndex)
            {
                const T chirpOffset =
                    static_cast<T>(pathIndex) * static_cast<T>(0.27)
                    + static_cast<T>(diffuserIndex) * static_cast<T>(0.11);

                path.dispersers[diffuserIndex].setDelaySamples(
                    millisecondsToDispersionSamples(
                        (kBaseDispersionMs[diffuserIndex] * dispersionScale)
                        + chirpOffset));

                const T polarity =
                    ((pathIndex + diffuserIndex) % 2u == 0u)
                        ? static_cast<T>(1)
                        : static_cast<T>(-1);

                path.dispersers[diffuserIndex].setFeedback(
                    polarity
                    * (static_cast<T>(0.48)
                       + (tension_ * static_cast<T>(0.22))
                       - (static_cast<T>(diffuserIndex) * static_cast<T>(0.035))));
            }
        }
    }

    void updateFeedbackGain() noexcept
    {
        feedbackGain_ =
            static_cast<T>(0.44)
            + (springLength_ * static_cast<T>(0.26))
            - (tension_ * static_cast<T>(0.08));

        feedbackGain_ =
            std::clamp(
                feedbackGain_,
                static_cast<T>(0.32),
                static_cast<T>(0.72));
    }

    void updateToneFilters() noexcept
    {
        const T centerHz =
            static_cast<T>(760)
            + (tone_ * static_cast<T>(1650));

        const T inputQ =
            static_cast<T>(0.95)
            + (tone_ * static_cast<T>(0.75));

        const T outputQ =
            static_cast<T>(0.75)
            + (tone_ * static_cast<T>(0.55));

        inputBandpass_.setType(cvdsp::filters::BiquadType::BandPass);
        inputBandpass_.setFrequency(centerHz);
        inputBandpass_.setQ(inputQ);
        inputBandpass_.updateCoefficients();

        outputTone_.setType(cvdsp::filters::BiquadType::BandPass);
        outputTone_.setFrequency(
            centerHz * static_cast<T>(1.12));
        outputTone_.setQ(outputQ);
        outputTone_.updateCoefficients();

        const T dampingCutoff =
            static_cast<T>(1800)
            + (tone_ * static_cast<T>(5200));

        for (auto& path : paths_)
        {
            path.damping.setCutoffHz(dampingCutoff);
        }
    }

    [[nodiscard]]
    T processPath(
        T excitation,
        SpringPath& path) noexcept
    {
        const T delayed =
            path.delay.readIntegerSamples(path.delaySamples);

        const T damped = path.damping.process(delayed);

        T dispersed =
            (excitation * path.inputGain)
            + (damped * feedbackGain_);

        for (auto& disperser : path.dispersers)
        {
            dispersed = disperser.process(dispersed);
        }

        path.delay.write(dispersed);

        return delayed * path.outputGain;
    }

    [[nodiscard]]
    T processSample(
        T input) noexcept
    {
        const T excitation = inputBandpass_.process(input);

        const T path0 = processPath(excitation, paths_[0]);
        const T path1 = processPath(excitation, paths_[1]);
        const T path2 = processPath(excitation, paths_[2]);

        const T tankOutput =
            (path0 + path1 + path2)
            * static_cast<T>(0.42);

        return outputTone_.process(tankOutput);
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

    std::array<SpringPath, kNumPaths> paths_{};
    cvdsp::filters::Biquad<T> inputBandpass_{};
    cvdsp::filters::Biquad<T> outputTone_{};

    T sampleRate_ = static_cast<T>(44100);
    T springLength_ = static_cast<T>(0.62);
    T tension_ = static_cast<T>(0.55);
    T tone_ = static_cast<T>(0.58);
    T mix_ = static_cast<T>(0.22);
    T feedbackGain_ = static_cast<T>(0.56);

    size_type numChannels_ = 0;
    bool prepared_ = false;
};

} // namespace cvdsp::reverb

#endif // CVDSP_REVERB_SPRINGREVERB_HPP
