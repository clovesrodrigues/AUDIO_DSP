#ifndef CVDSP_SPECTRAL_REALTIMENOISEREDUCER_HPP
#define CVDSP_SPECTRAL_REALTIMENOISEREDUCER_HPP

/**
 * @file RealtimeNoiseReducer.hpp
 * @brief Minimal real-time noise profile learner/reducer for recording chains.
 *
 * Header-only, allocation-free after construction and instance-local by design.
 * This core is intentionally simpler than SpectralNoiseReducer: it exposes a
 * learn/apply workflow plus gain, presence protection and smoothing controls.
 */

#include "FFT.hpp"
#include "WindowFunctions.hpp"

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cmath>
#include <type_traits>

namespace cvdsp::spectral
{

enum class RealtimeNoiseReducerState
{
    Idle,
    Learning,
    Reducing
};

template<typename T = float, std::size_t FFTSize = 2048>
class RealtimeNoiseReducer
{
    static_assert(std::is_floating_point_v<T>, "RealtimeNoiseReducer requires a floating point type");
    static_assert((FFTSize & (FFTSize - 1)) == 0, "FFTSize must be a power of two");
    static_assert(FFTSize >= 512 && FFTSize <= FFT<T>::MaxFFTSize,
                  "FFTSize must be supported by cvdsp::spectral::FFT");

public:
    using Complex = std::complex<T>;

    static constexpr std::size_t kFFTSize = FFTSize;
    static constexpr std::size_t kHopSize = FFTSize / 2;
    static constexpr std::size_t kNumBins = FFTSize / 2 + 1;
    static constexpr std::size_t kOutputBufferSize = FFTSize + kHopSize;

    constexpr RealtimeNoiseReducer() noexcept = default;

    bool prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(44100);
        prepared_ = fft_.prepare(FFTSize);
        window_ = WindowFunctions<T, FFTSize>::generate(WindowType::Hann);
        updateSynthesisGain();
        reset();
        return prepared_;
    }

    void reset() noexcept
    {
        learnEnabled_ = false;
        reduceEnabled_ = false;
        clearProfile();
        resetStreamingState();
        bandTargetGain_.fill(static_cast<T>(1));
        bandSmoothedGain_.fill(static_cast<T>(1));
        bandSignalPower_.fill(static_cast<T>(0));
        bandProfilePower_.fill(static_cast<T>(0));
    }

    void clearProfile() noexcept
    {
        noisePowerSum_.fill(static_cast<T>(0));
        noisePower_.fill(static_cast<T>(0));
        bandTargetGain_.fill(static_cast<T>(1));
        bandSmoothedGain_.fill(static_cast<T>(1));
        bandSignalPower_.fill(static_cast<T>(0));
        bandProfilePower_.fill(static_cast<T>(0));
        learnedFrameCount_ = 0;
        profileReady_ = false;
        resetStreamingState();
    }

    void triggerClearProfile() noexcept { clearProfile(); }

    /**
     * @brief Clears overlap-add/input history without deleting the learned noise profile.
     *
     * Use this when a host toggles learn/reduce states so stale FFT latency from a
     * previous mode cannot leak into the next realtime pass.
     */
    void resetLatencyState() noexcept
    {
        resetStreamingState();
        bandTargetGain_.fill(static_cast<T>(1));
        bandSmoothedGain_.fill(static_cast<T>(1));
    }

    void setLearnNoiseEnabled(bool enabled) noexcept { learnEnabled_ = enabled; }
    void setSubtractNoiseEnabled(bool enabled) noexcept { reduceEnabled_ = enabled; }

    void setOutputGainDb(T db) noexcept
    {
        outputGainDb_ = std::clamp(db, static_cast<T>(-24), static_cast<T>(24));
        outputGain_ = dbToGain(outputGainDb_);
    }

    void setPresenceProtect(T normalized) noexcept { presenceProtect_ = clamp01(normalized); }
    void setSmoothing(T normalized) noexcept { smoothing_ = clamp01(normalized) * static_cast<T>(0.98); }

    void setMinimumLearnFrames(std::size_t frames) noexcept
    {
        minimumLearnFrames_ = std::max<std::size_t>(1, frames);
        profileReady_ = learnedFrameCount_ >= minimumLearnFrames_;
    }

    [[nodiscard]] T processSample(T input) noexcept
    {
        if (!prepared_ || (!learnEnabled_ && !reduceEnabled_))
            return input;

        if (!learnEnabled_ && reduceEnabled_ && !profileReady_)
            return input;

        const T output = processStreamingSample(input);
        return reduceEnabled_ && profileReady_ ? output : input;
    }

    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] bool isProfileReady() const noexcept { return profileReady_; }
    [[nodiscard]] bool isLearning() const noexcept { return learnEnabled_; }
    [[nodiscard]] bool isReducing() const noexcept { return reduceEnabled_ && profileReady_; }
    [[nodiscard]] std::size_t getLearnedFrameCount() const noexcept { return learnedFrameCount_; }
    [[nodiscard]] T getLearnProgress() const noexcept
    {
        return std::min(static_cast<T>(1), static_cast<T>(learnedFrameCount_) / static_cast<T>(minimumLearnFrames_));
    }
    [[nodiscard]] T getOutputGainDb() const noexcept { return outputGainDb_; }
    [[nodiscard]] T getPresenceProtect() const noexcept { return presenceProtect_; }
    [[nodiscard]] T getSmoothing() const noexcept { return smoothing_ / static_cast<T>(0.98); }
    [[nodiscard]] RealtimeNoiseReducerState getState() const noexcept
    {
        if (learnEnabled_)
            return RealtimeNoiseReducerState::Learning;
        if (reduceEnabled_ && profileReady_)
            return RealtimeNoiseReducerState::Reducing;
        return RealtimeNoiseReducerState::Idle;
    }

private:
    static constexpr T kPowerFloor = static_cast<T>(1e-20);
    static constexpr T kReductionAmount = static_cast<T>(0.72);
    static constexpr T kSpectralFloorGain = static_cast<T>(0.02);
    static constexpr T kMaxReductionGain = static_cast<T>(0.01584893192461113); // -36 dB
    static constexpr std::size_t kBandCount = FFTSize <= 512 ? 24 : 32;
    static constexpr std::size_t kBinsPerBand = (kNumBins + kBandCount - 1) / kBandCount;

    [[nodiscard]] static T clamp01(T value) noexcept
    {
        return std::clamp(value, static_cast<T>(0), static_cast<T>(1));
    }

    [[nodiscard]] static T dbToGain(T db) noexcept
    {
        return std::pow(static_cast<T>(10), db / static_cast<T>(20));
    }

    void resetStreamingState() noexcept
    {
        inputRing_.fill(static_cast<T>(0));
        outputRing_.fill(static_cast<T>(0));
        frame_.fill(Complex(static_cast<T>(0), static_cast<T>(0)));
        inputWriteIndex_ = 0;
        outputReadIndex_ = 0;
        hopCounter_ = 0;
    }

    [[nodiscard]] T processStreamingSample(T input) noexcept
    {
        inputRing_[inputWriteIndex_] = input;
        inputWriteIndex_ = (inputWriteIndex_ + 1) % FFTSize;

        const T output = outputRing_[outputReadIndex_];
        outputRing_[outputReadIndex_] = static_cast<T>(0);

        ++hopCounter_;
        if (hopCounter_ >= kHopSize)
        {
            hopCounter_ = 0;
            processFrame(outputReadIndex_);
        }

        outputReadIndex_ = (outputReadIndex_ + 1) % kOutputBufferSize;
        return output;
    }

    void processFrame(std::size_t outputStartIndex) noexcept
    {
        for (std::size_t i = 0; i < FFTSize; ++i)
        {
            const std::size_t ringIndex = (inputWriteIndex_ + i) % FFTSize;
            frame_[i] = Complex(inputRing_[ringIndex] * window_[i], static_cast<T>(0));
        }

        fft_.forward(frame_.data());

        if (learnEnabled_)
            learnFrame();

        if (!reduceEnabled_ || !profileReady_)
            return;

        reduceFrame();
        fft_.inverse(frame_.data());

        for (std::size_t i = 0; i < FFTSize; ++i)
        {
            const std::size_t outIndex = (outputStartIndex + i) % kOutputBufferSize;
            outputRing_[outIndex] += frame_[i].real() * window_[i] * synthesisGain_;
        }
    }

    void learnFrame() noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            noisePowerSum_[bin] += power(frame_[bin]);

        if (learnedFrameCount_ < static_cast<std::size_t>(-1))
            ++learnedFrameCount_;

        const T reciprocalFrames = static_cast<T>(1) / static_cast<T>(std::max<std::size_t>(1, learnedFrameCount_));
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            noisePower_[bin] = noisePowerSum_[bin] * reciprocalFrames;

        profileReady_ = learnedFrameCount_ >= minimumLearnFrames_;
    }

    void reduceFrame() noexcept
    {
        buildBandGainFrame();
        smoothBandGains();

        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            const T gain = bandSmoothedGain_[bandForBin(bin)] * outputGain_;
            const Complex processed = frame_[bin] * gain;
            frame_[bin] = processed;
            mirrorConjugate(bin, processed);
        }
    }

    void buildBandGainFrame() noexcept
    {
        bandSignalPower_.fill(static_cast<T>(0));
        bandProfilePower_.fill(static_cast<T>(0));

        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            const std::size_t band = bandForBin(bin);
            bandSignalPower_[band] += power(frame_[bin]);
            bandProfilePower_[band] += noisePower_[bin];
        }

        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const T signalPower = std::max(bandSignalPower_[band], kPowerFloor);
            const T profilePower = bandProfilePower_[band];
            const T excessRatio = std::max(signalPower - profilePower, static_cast<T>(0)) / (signalPower + kPowerFloor);
            const T wienerGain = signalPower / (signalPower + kReductionAmount * profilePower + kPowerFloor);
            const T protectedGain = std::max(wienerGain, excessRatio * presenceScale(centerBinForBand(band)));
            const T minimumGain = std::max(kSpectralFloorGain, kMaxReductionGain);
            bandTargetGain_[band] = std::clamp(std::max(protectedGain, minimumGain), static_cast<T>(0), static_cast<T>(1));
        }
    }

    void smoothBandGains() noexcept
    {
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const std::size_t first = band > 0 ? band - 1 : 0;
            const std::size_t last = std::min<std::size_t>(kBandCount - 1, band + 1);

            T weightedSum = static_cast<T>(0);
            T weightSum = static_cast<T>(0);
            for (std::size_t neighbor = first; neighbor <= last; ++neighbor)
            {
                const std::size_t distance = neighbor > band ? neighbor - band : band - neighbor;
                const T weight = distance == 0 ? static_cast<T>(2) : static_cast<T>(1);
                weightedSum += bandTargetGain_[neighbor] * weight;
                weightSum += weight;
            }

            const T frequencySmoothedGain = weightSum > static_cast<T>(0)
                ? std::clamp(weightedSum / weightSum, static_cast<T>(0), static_cast<T>(1))
                : bandTargetGain_[band];
            const T smoothed = smoothing_ * bandSmoothedGain_[band]
                + (static_cast<T>(1) - smoothing_) * frequencySmoothedGain;
            bandSmoothedGain_[band] = std::clamp(smoothed, static_cast<T>(0), static_cast<T>(1));
        }
    }

    [[nodiscard]] static constexpr std::size_t bandForBin(std::size_t bin) noexcept
    {
        return std::min<std::size_t>(kBandCount - 1, bin / kBinsPerBand);
    }

    [[nodiscard]] static constexpr std::size_t centerBinForBand(std::size_t band) noexcept
    {
        const std::size_t first = band * kBinsPerBand;
        const std::size_t last = std::min<std::size_t>(kNumBins - 1, first + kBinsPerBand - 1);
        return (first + last) / 2;
    }

    [[nodiscard]] T presenceScale(std::size_t bin) const noexcept
    {
        if (presenceProtect_ <= static_cast<T>(0))
            return static_cast<T>(1);

        const T frequency = static_cast<T>(bin) * sampleRate_ / static_cast<T>(FFTSize);
        T weight = static_cast<T>(0);
        if (frequency >= static_cast<T>(1500) && frequency <= static_cast<T>(6000))
            weight = static_cast<T>(1);
        else if (frequency >= static_cast<T>(800) && frequency < static_cast<T>(1500))
            weight = (frequency - static_cast<T>(800)) / static_cast<T>(700);
        else if (frequency > static_cast<T>(6000) && frequency <= static_cast<T>(10000))
            weight = (static_cast<T>(10000) - frequency) / static_cast<T>(4000);

        return std::clamp(static_cast<T>(1) + presenceProtect_ * static_cast<T>(0.75) * clamp01(weight),
                          static_cast<T>(1),
                          static_cast<T>(1.75));
    }

    void mirrorConjugate(std::size_t bin, const Complex& value) noexcept
    {
        if (bin == 0 || bin == FFTSize / 2)
            return;
        frame_[FFTSize - bin] = std::conj(value);
    }

    [[nodiscard]] static T power(const Complex& c) noexcept
    {
        return c.real() * c.real() + c.imag() * c.imag();
    }

    void updateSynthesisGain() noexcept
    {
        T maxWindowSum = static_cast<T>(0);
        for (std::size_t offset = 0; offset < kHopSize; ++offset)
        {
            T sum = static_cast<T>(0);
            for (std::size_t i = offset; i < FFTSize; i += kHopSize)
                sum += window_[i] * window_[i];
            maxWindowSum = std::max(maxWindowSum, sum);
        }

        synthesisGain_ = maxWindowSum > static_cast<T>(0)
            ? static_cast<T>(1) / maxWindowSum
            : static_cast<T>(1);
    }

    T sampleRate_ { static_cast<T>(44100) };
    T outputGainDb_ { static_cast<T>(0) };
    T outputGain_ { static_cast<T>(1) };
    T presenceProtect_ { static_cast<T>(0.5) };
    T smoothing_ { static_cast<T>(0.637) };
    T synthesisGain_ { static_cast<T>(1) };

    bool prepared_ { false };
    bool learnEnabled_ { false };
    bool reduceEnabled_ { false };
    bool profileReady_ { false };
    std::size_t minimumLearnFrames_ { 6 };
    std::size_t learnedFrameCount_ { 0 };

    FFT<T> fft_ {};
    typename WindowFunctions<T, FFTSize>::WindowBuffer window_ {};
    std::array<T, FFTSize> inputRing_ {};
    std::array<T, kOutputBufferSize> outputRing_ {};
    std::array<Complex, FFTSize> frame_ {};
    std::size_t inputWriteIndex_ { 0 };
    std::size_t outputReadIndex_ { 0 };
    std::size_t hopCounter_ { 0 };

    std::array<T, kNumBins> noisePowerSum_ {};
    std::array<T, kNumBins> noisePower_ {};
    std::array<T, kBandCount> bandSignalPower_ {};
    std::array<T, kBandCount> bandProfilePower_ {};
    std::array<T, kBandCount> bandTargetGain_ {};
    std::array<T, kBandCount> bandSmoothedGain_ {};
};

using RealtimeNoiseReducer512F = RealtimeNoiseReducer<float, 512>;
using RealtimeNoiseReducer1024F = RealtimeNoiseReducer<float, 1024>;
using RealtimeNoiseReducer2048F = RealtimeNoiseReducer<float, 2048>;
using RealtimeNoiseReducer4096F = RealtimeNoiseReducer<float, 4096>;

} // namespace cvdsp::spectral

#endif // CVDSP_SPECTRAL_REALTIMENOISEREDUCER_HPP
