#ifndef CVDSP_SPECTRAL_SPECTRALNOISEREDUCER_HPP
#define CVDSP_SPECTRAL_SPECTRALNOISEREDUCER_HPP

/**
 * @file SpectralNoiseReducer.hpp
 * @brief Frame-based spectral noise profile learner and subtractor.
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Provides both frame-domain spectrum processing and a streaming STFT path with
 * fixed-size buffers for processSample/processBlock use.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <type_traits>

#include "FFT.hpp"
#include "WindowFunctions.hpp"
#include "../Core/AudioBufferView.hpp"

namespace cvdsp::spectral
{

/**
 * @brief State derived from LearnNoise and SubtractNoise switches.
 */
enum class SpectralNoiseReducerState
{
    Idle,
    Learn,
    Subtract,
    LearnAndSubtract
};

/**
 * @brief Learns a spectral noise profile and subtracts it from FFT frames.
 *
 * Callers may either pass full FFT frames via processSpectrum() or feed
 * time-domain audio through processSample()/processBlock(). The streaming path
 * uses fixed-size Hann-windowed STFT buffers, overlap-add synthesis and the same
 * learned profile/subtraction core used by the frame-domain API. No dynamic
 * memory is allocated by the hot path.
 *
 * @tparam T Floating point sample type.
 * @tparam FFTSize FFT frame size. Supported sizes mirror CV_DSP::spectral::FFT.
 * @tparam OverlapPercent STFT overlap percentage: 25, 50 or 75.
 */
template<typename T = float, std::size_t FFTSize = 2048, std::size_t OverlapPercent = 75>
class SpectralNoiseReducer
{
    static_assert(std::is_floating_point_v<T>, "SpectralNoiseReducer requires a floating point type");
    static_assert(
        FFTSize == 512 || FFTSize == 1024 || FFTSize == 2048 || FFTSize == 4096 || FFTSize == 8192,
        "SpectralNoiseReducer supports FFT sizes 512, 1024, 2048, 4096 and 8192");
    static_assert(
        OverlapPercent == 25 || OverlapPercent == 50 || OverlapPercent == 75,
        "SpectralNoiseReducer overlap must be 25, 50 or 75 percent");

public:
    using value_type = T;
    using Complex = std::complex<T>;

    static constexpr std::size_t kFFTSize = FFTSize;
    static constexpr std::size_t kHopSize = FFTSize * (100 - OverlapPercent) / 100;
    static constexpr std::size_t kNumBins = FFTSize / 2 + 1;
    static constexpr std::size_t kOutputBufferSize = FFTSize * 2;
    static constexpr T kGuiMinimumDb = static_cast<T>(-120);
    static constexpr T kGuiMaximumDb = static_cast<T>(12);
    using SpectrumView = std::array<T, kNumBins>;

    /**
     * @brief Fixed-size display snapshot for CV_GUI or host-side spectrum views.
     *
     * The raw magnitude snapshots remain available through getInputSpectrum(),
     * getNoiseProfile(), getOutputSpectrum() and getReductionCurve(). This view
     * precomputes dB and normalized 0..1 values so a GUI can draw before/after
     * curves without allocating or doing expensive mapping work in the paint path.
     */
    struct GuiSnapshot
    {
        SpectrumView inputDb {};
        SpectrumView noiseProfileDb {};
        SpectrumView outputDb {};
        SpectrumView reductionDb {};
        SpectrumView inputNormalized {};
        SpectrumView noiseProfileNormalized {};
        SpectrumView outputNormalized {};
        SpectrumView reductionNormalized {};
        T averageReductionDb { static_cast<T>(0) };
        T learnProgress { static_cast<T>(0) };
        T sampleRate { static_cast<T>(44100) };
        std::size_t learnedFrameCount { 0 };
        std::size_t minimumLearnFrames { 1 };
        bool learning { false };
        bool subtracting { false };
        bool profileReady { false };
        SpectralNoiseReducerState state { SpectralNoiseReducerState::Idle };
    };

    constexpr SpectralNoiseReducer() noexcept = default;

    bool prepare(T sampleRate) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(44100);
        prepared_ = fft_.prepare(FFTSize);
        window_ = WindowFunctions<T, FFTSize>::generate(WindowType::Hann);
        updateSynthesisNormalization();
        reset();
        return prepared_;
    }

    void reset() noexcept
    {
        learnNoiseEnabled_ = false;
        subtractNoiseEnabled_ = false;
        clearProfile();
        resetStreamingState();
        inputSpectrum_.fill(static_cast<T>(0));
        outputSpectrum_.fill(static_cast<T>(0));
        reductionCurve_.fill(static_cast<T>(1));
        smoothedGain_.fill(static_cast<T>(1));
        targetGain_.fill(static_cast<T>(1));
        frequencySmoothedGain_.fill(static_cast<T>(1));
        averageReductionDb_ = static_cast<T>(0);
    }

    void clearProfile() noexcept
    {
        noiseProfileSum_.fill(static_cast<T>(0));
        noiseProfile_.fill(static_cast<T>(0));
        smoothedGain_.fill(static_cast<T>(1));
        targetGain_.fill(static_cast<T>(1));
        frequencySmoothedGain_.fill(static_cast<T>(1));
        reductionCurve_.fill(static_cast<T>(1));
        learnedFrameCount_ = 0;
        profileReady_ = false;
    }

    void triggerClearProfile() noexcept
    {
        clearProfile();
    }

    void setLearnNoiseEnabled(bool enabled) noexcept
    {
        learnNoiseEnabled_ = enabled;
    }

    void setSubtractNoiseEnabled(bool enabled) noexcept
    {
        subtractNoiseEnabled_ = enabled;
    }

    void setOutputGainDb(T db) noexcept
    {
        outputGainDb_ = std::clamp(db, static_cast<T>(-24), static_cast<T>(24));
        outputGain_ = dbToGain(outputGainDb_);
    }

    void setPresenceProtect(T normalized) noexcept
    {
        presenceProtect_ = clamp01(normalized);
    }

    void setReductionAmount(T normalized) noexcept
    {
        reductionAmount_ = clamp01(normalized);
    }

    void setSpectralFloorDb(T db) noexcept
    {
        spectralFloorDb_ = std::clamp(db, static_cast<T>(-120), static_cast<T>(-12));
        spectralFloorGain_ = dbToGain(spectralFloorDb_);
    }

    void setMaxReductionDb(T db) noexcept
    {
        maxReductionDb_ = std::clamp(db, static_cast<T>(0), static_cast<T>(80));
        maxReductionGain_ = dbToGain(-maxReductionDb_);
    }

    void setSmoothing(T normalized) noexcept
    {
        smoothing_ = clamp01(normalized) * static_cast<T>(0.98);
    }

    void setFrequencySmoothingBins(std::size_t bins) noexcept
    {
        frequencySmoothingBins_ = std::min<std::size_t>(bins, kMaxFrequencySmoothingBins);
    }

    void setTransientProtection(T normalized) noexcept
    {
        transientProtection_ = clamp01(normalized);
    }

    void setMix(T normalized) noexcept
    {
        mix_ = clamp01(normalized);
    }

    void setMinimumLearnFrames(std::size_t frames) noexcept
    {
        minimumLearnFrames_ = std::max<std::size_t>(1, frames);
        profileReady_ = learnedFrameCount_ >= minimumLearnFrames_;
    }

    /**
     * @brief Process one time-domain sample through the streaming STFT path.
     *
     * When both LearnNoise and SubtractNoise are disabled this is a true
     * pass-through. Learn-only mode analyzes frames while returning the original
     * input. Subtract mode returns the overlap-add output once a profile is ready.
     */
    [[nodiscard]] T processSample(T input) noexcept
    {
        if (!prepared_)
            return input;

        const bool needsSpectralPath = learnNoiseEnabled_ || subtractNoiseEnabled_;
        if (!needsSpectralPath)
            return input;

        if (!learnNoiseEnabled_ && subtractNoiseEnabled_ && !profileReady_)
            return input;

        const T processed = processStreamingSample(input);
        if (subtractNoiseEnabled_ && profileReady_)
            return processed;

        return input;
    }

    void processBlock(AudioBufferView<T> buffer) noexcept
    {
        if (!buffer.isValid())
            return;

        for (typename AudioBufferView<T>::size_type channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            T* channelData = buffer.getChannel(channel);
            resetStreamingState();
            for (typename AudioBufferView<T>::size_type sample = 0; sample < buffer.getNumSamples(); ++sample)
                channelData[sample] = processSample(channelData[sample]);
        }
    }

    /**
     * @brief Process a full complex FFT frame in-place.
     *
     * The frame is expected to contain a real-signal spectrum. Positive bins are
     * processed, then mirrored into negative bins via conjugate symmetry.
     */
    void processSpectrum(Complex* spectrum) noexcept
    {
        if (spectrum == nullptr)
            return;

        captureInputSpectrum(spectrum);

        if (learnNoiseEnabled_)
            learnProfile(spectrum);

        if (subtractNoiseEnabled_ && profileReady_)
        {
            subtractProfile(spectrum);
        }
        else
        {
            copyInputToOutputSpectrum();
            reductionCurve_.fill(static_cast<T>(1));
            averageReductionDb_ = static_cast<T>(0);
        }
    }

    /**
     * @brief Process split real/imaginary positive-frequency FFT bins in-place.
     *
     * The arrays must contain at least kNumBins elements. This overload is useful
     * for adapters that expose split spectra instead of std::complex buffers.
     */
    void processSpectrum(T* fftReal, T* fftImag) noexcept
    {
        if (fftReal == nullptr || fftImag == nullptr)
            return;

        captureInputSpectrum(fftReal, fftImag);

        if (learnNoiseEnabled_)
            learnProfile(fftReal, fftImag);

        if (subtractNoiseEnabled_ && profileReady_)
            subtractProfile(fftReal, fftImag);
        else
        {
            copyInputToOutputSpectrum();
            reductionCurve_.fill(static_cast<T>(1));
            averageReductionDb_ = static_cast<T>(0);
        }
    }

    [[nodiscard]] bool isLearning() const noexcept { return learnNoiseEnabled_; }
    [[nodiscard]] bool isSubtracting() const noexcept { return subtractNoiseEnabled_; }
    [[nodiscard]] bool isProfileReady() const noexcept { return profileReady_; }
    [[nodiscard]] std::size_t getLearnedFrameCount() const noexcept { return learnedFrameCount_; }
    [[nodiscard]] std::size_t getMinimumLearnFrames() const noexcept { return minimumLearnFrames_; }
    [[nodiscard]] T getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] T getOutputGainDb() const noexcept { return outputGainDb_; }
    [[nodiscard]] T getPresenceProtect() const noexcept { return presenceProtect_; }
    [[nodiscard]] T getReductionAmount() const noexcept { return reductionAmount_; }
    [[nodiscard]] T getSpectralFloorDb() const noexcept { return spectralFloorDb_; }
    [[nodiscard]] T getMaxReductionDb() const noexcept { return maxReductionDb_; }
    [[nodiscard]] T getSmoothing() const noexcept { return smoothing_ / static_cast<T>(0.98); }
    [[nodiscard]] std::size_t getFrequencySmoothingBins() const noexcept { return frequencySmoothingBins_; }
    [[nodiscard]] T getTransientProtection() const noexcept { return transientProtection_; }
    [[nodiscard]] T getMix() const noexcept { return mix_; }
    [[nodiscard]] T getAverageReductionDb() const noexcept { return averageReductionDb_; }
    [[nodiscard]] static constexpr std::size_t getHopSize() noexcept { return kHopSize; }
    [[nodiscard]] static constexpr std::size_t getLatencySamples() noexcept { return FFTSize; }

    [[nodiscard]] T getLearnProgress() const noexcept
    {
        return std::clamp(
            static_cast<T>(learnedFrameCount_) / static_cast<T>(std::max<std::size_t>(1, minimumLearnFrames_)),
            static_cast<T>(0),
            static_cast<T>(1));
    }

    [[nodiscard]] SpectralNoiseReducerState getState() const noexcept
    {
        if (learnNoiseEnabled_ && subtractNoiseEnabled_)
            return SpectralNoiseReducerState::LearnAndSubtract;
        if (learnNoiseEnabled_)
            return SpectralNoiseReducerState::Learn;
        if (subtractNoiseEnabled_)
            return SpectralNoiseReducerState::Subtract;
        return SpectralNoiseReducerState::Idle;
    }

    [[nodiscard]] const SpectrumView& getInputSpectrum() const noexcept { return inputSpectrum_; }
    [[nodiscard]] const SpectrumView& getNoiseProfile() const noexcept { return noiseProfile_; }
    [[nodiscard]] const SpectrumView& getOutputSpectrum() const noexcept { return outputSpectrum_; }
    [[nodiscard]] const SpectrumView& getReductionCurve() const noexcept { return reductionCurve_; }

    [[nodiscard]] T getBinFrequencyHz(std::size_t bin) const noexcept
    {
        const std::size_t clampedBin = std::min(bin, kNumBins - 1);
        return static_cast<T>(clampedBin) * sampleRate_ / static_cast<T>(FFTSize);
    }

    void fillGuiSnapshot(GuiSnapshot& snapshot) const noexcept
    {
        snapshot.averageReductionDb = averageReductionDb_;
        snapshot.learnProgress = getLearnProgress();
        snapshot.sampleRate = sampleRate_;
        snapshot.learnedFrameCount = learnedFrameCount_;
        snapshot.minimumLearnFrames = minimumLearnFrames_;
        snapshot.learning = learnNoiseEnabled_;
        snapshot.subtracting = subtractNoiseEnabled_;
        snapshot.profileReady = profileReady_;
        snapshot.state = getState();

        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            snapshot.inputDb[bin] = magnitudeToGuiDb(inputSpectrum_[bin]);
            snapshot.noiseProfileDb[bin] = magnitudeToGuiDb(noiseProfile_[bin]);
            snapshot.outputDb[bin] = magnitudeToGuiDb(outputSpectrum_[bin]);
            snapshot.reductionDb[bin] = gainToGuiDb(reductionCurve_[bin]);

            snapshot.inputNormalized[bin] = dbToGuiNormalized(snapshot.inputDb[bin]);
            snapshot.noiseProfileNormalized[bin] = dbToGuiNormalized(snapshot.noiseProfileDb[bin]);
            snapshot.outputNormalized[bin] = dbToGuiNormalized(snapshot.outputDb[bin]);
            snapshot.reductionNormalized[bin] = dbToGuiNormalized(snapshot.reductionDb[bin]);
        }
    }

private:
    static constexpr T kMagnitudeFloor = static_cast<T>(1e-20);
    static constexpr std::size_t kMaxFrequencySmoothingBins = 12;

    [[nodiscard]] static T clamp01(T value) noexcept
    {
        return std::clamp(value, static_cast<T>(0), static_cast<T>(1));
    }

    [[nodiscard]] static T dbToGain(T db) noexcept
    {
        return std::pow(static_cast<T>(10), db / static_cast<T>(20));
    }

    [[nodiscard]] static T gainToDb(T gain) noexcept
    {
        return static_cast<T>(20) * std::log10(std::max(gain, kMagnitudeFloor));
    }

    [[nodiscard]] static T magnitudeToGuiDb(T magnitudeValue) noexcept
    {
        return std::clamp(gainToDb(magnitudeValue), kGuiMinimumDb, kGuiMaximumDb);
    }

    [[nodiscard]] static T gainToGuiDb(T gain) noexcept
    {
        return std::clamp(gainToDb(gain), kGuiMinimumDb, static_cast<T>(0));
    }

    [[nodiscard]] static T dbToGuiNormalized(T db) noexcept
    {
        const T clampedDb = std::clamp(db, kGuiMinimumDb, kGuiMaximumDb);
        return (clampedDb - kGuiMinimumDb) / (kGuiMaximumDb - kGuiMinimumDb);
    }

    [[nodiscard]] static T magnitude(const Complex& value) noexcept
    {
        return std::sqrt(value.real() * value.real() + value.imag() * value.imag());
    }

    void captureInputSpectrum(const Complex* spectrum) noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            inputSpectrum_[bin] = magnitude(spectrum[bin]);
    }

    void captureInputSpectrum(const T* fftReal, const T* fftImag) noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            inputSpectrum_[bin] = std::sqrt(fftReal[bin] * fftReal[bin] + fftImag[bin] * fftImag[bin]);
    }

    void copyInputToOutputSpectrum() noexcept
    {
        outputSpectrum_ = inputSpectrum_;
    }

    void resetStreamingState() noexcept
    {
        inputRing_.fill(static_cast<T>(0));
        outputRing_.fill(static_cast<T>(0));
        timeFrame_.fill(static_cast<T>(0));
        spectrumFrame_.fill(Complex(static_cast<T>(0), static_cast<T>(0)));
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
            processStreamingFrame(outputReadIndex_);
        }

        outputReadIndex_ = (outputReadIndex_ + 1) % kOutputBufferSize;
        return output;
    }

    void processStreamingFrame(std::size_t outputStartIndex) noexcept
    {
        for (std::size_t i = 0; i < FFTSize; ++i)
        {
            const std::size_t ringIndex = (inputWriteIndex_ + i) % FFTSize;
            timeFrame_[i] = inputRing_[ringIndex] * window_[i];
            spectrumFrame_[i] = Complex(timeFrame_[i], static_cast<T>(0));
        }

        fft_.forward(spectrumFrame_.data());
        processSpectrum(spectrumFrame_.data());

        if (!subtractNoiseEnabled_ || !profileReady_)
            return;

        fft_.inverse(spectrumFrame_.data());

        for (std::size_t i = 0; i < FFTSize; ++i)
        {
            const std::size_t outIndex = (outputStartIndex + i) % kOutputBufferSize;
            outputRing_[outIndex] += spectrumFrame_[i].real() * window_[i] * synthesisGain_;
        }
    }

    void updateSynthesisNormalization() noexcept
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

    void learnProfile(const Complex* spectrum) noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            noiseProfileSum_[bin] += magnitude(spectrum[bin]);

        finishLearningFrame();
    }

    void learnProfile(const T* fftReal, const T* fftImag) noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            noiseProfileSum_[bin] += std::sqrt(fftReal[bin] * fftReal[bin] + fftImag[bin] * fftImag[bin]);

        finishLearningFrame();
    }

    void finishLearningFrame() noexcept
    {
        if (learnedFrameCount_ < static_cast<std::size_t>(-1))
            ++learnedFrameCount_;

        const T reciprocalFrames = static_cast<T>(1) / static_cast<T>(std::max<std::size_t>(1, learnedFrameCount_));
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            noiseProfile_[bin] = noiseProfileSum_[bin] * reciprocalFrames;

        profileReady_ = learnedFrameCount_ >= minimumLearnFrames_;
    }

    void subtractProfile(Complex* spectrum) noexcept
    {
        buildTargetGainFrame(spectrum);
        smoothGainFrameAcrossFrequency();

        T reductionDbSum = static_cast<T>(0);
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            const Complex original = spectrum[bin];
            const Complex processed = applyPreparedGainToBin(original, bin, reductionDbSum);
            spectrum[bin] = processed;
            mirrorConjugateBin(spectrum, bin, processed);
        }

        averageReductionDb_ = reductionDbSum / static_cast<T>(kNumBins);
    }

    void subtractProfile(T* fftReal, T* fftImag) noexcept
    {
        buildTargetGainFrame(fftReal, fftImag);
        smoothGainFrameAcrossFrequency();

        T reductionDbSum = static_cast<T>(0);
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            const Complex original(fftReal[bin], fftImag[bin]);
            const Complex processed = applyPreparedGainToBin(original, bin, reductionDbSum);
            fftReal[bin] = processed.real();
            fftImag[bin] = processed.imag();
        }

        averageReductionDb_ = reductionDbSum / static_cast<T>(kNumBins);
    }

    void buildTargetGainFrame(const Complex* spectrum) noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
            targetGain_[bin] = computeTargetGain(magnitude(spectrum[bin]), noiseProfile_[bin], bin);
    }

    void buildTargetGainFrame(const T* fftReal, const T* fftImag) noexcept
    {
        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            const T inputMagnitude = std::sqrt(fftReal[bin] * fftReal[bin] + fftImag[bin] * fftImag[bin]);
            targetGain_[bin] = computeTargetGain(inputMagnitude, noiseProfile_[bin], bin);
        }
    }

    [[nodiscard]] T computeTargetGain(T inputMagnitude, T profileMagnitude, std::size_t bin) const noexcept
    {
        if (inputMagnitude <= kMagnitudeFloor)
            return static_cast<T>(1);

        const T effectiveReduction = reductionAmount_ * static_cast<T>(3) * presenceReductionScale(bin);
        const T rawMagnitude = inputMagnitude - effectiveReduction * profileMagnitude;
        const T minimumMagnitude = inputMagnitude * std::max(spectralFloorGain_, maxReductionGain_);
        const T targetMagnitude = std::max(rawMagnitude, minimumMagnitude);
        T targetGain = std::clamp(targetMagnitude / inputMagnitude, static_cast<T>(0), static_cast<T>(1));

        // Audacity-style safety: bins well above the learned profile are likely
        // wanted signal, not stationary noise. Bias their gain back toward unity
        // before time/frequency smoothing to avoid isolated-bin robotic artifacts.
        const T signalExcess = inputMagnitude - profileMagnitude * (static_cast<T>(1) + transientProtection_);
        if (signalExcess > static_cast<T>(0))
        {
            const T denominator = inputMagnitude + profileMagnitude + kMagnitudeFloor;
            const T protect = clamp01(signalExcess / denominator);
            targetGain = std::max(targetGain, protect);
        }

        return targetGain;
    }

    void smoothGainFrameAcrossFrequency() noexcept
    {
        if (frequencySmoothingBins_ == 0)
        {
            frequencySmoothedGain_ = targetGain_;
            return;
        }

        for (std::size_t bin = 0; bin < kNumBins; ++bin)
        {
            const std::size_t first = bin > frequencySmoothingBins_ ? bin - frequencySmoothingBins_ : 0;
            const std::size_t last = std::min<std::size_t>(kNumBins - 1, bin + frequencySmoothingBins_);

            T weightedSum = static_cast<T>(0);
            T weightSum = static_cast<T>(0);
            for (std::size_t neighbor = first; neighbor <= last; ++neighbor)
            {
                const std::size_t distance = neighbor > bin ? neighbor - bin : bin - neighbor;
                const T weight = static_cast<T>(frequencySmoothingBins_ + 1 - distance);
                weightedSum += targetGain_[neighbor] * weight;
                weightSum += weight;
            }

            frequencySmoothedGain_[bin] = weightSum > static_cast<T>(0)
                ? std::clamp(weightedSum / weightSum, static_cast<T>(0), static_cast<T>(1))
                : targetGain_[bin];
        }
    }

    [[nodiscard]] Complex applyPreparedGainToBin(const Complex& original, std::size_t bin, T& reductionDbSum) noexcept
    {
        const T inputMagnitude = magnitude(original);
        if (inputMagnitude <= kMagnitudeFloor)
        {
            outputSpectrum_[bin] = static_cast<T>(0);
            reductionCurve_[bin] = static_cast<T>(1);
            smoothedGain_[bin] = static_cast<T>(1);
            return Complex(static_cast<T>(0), static_cast<T>(0));
        }

        const T targetGain = frequencySmoothedGain_[bin];
        const T smoothedGain = smoothing_ * smoothedGain_[bin] + (static_cast<T>(1) - smoothing_) * targetGain;
        smoothedGain_[bin] = std::clamp(smoothedGain, static_cast<T>(0), static_cast<T>(1));

        const Complex reduced = original * smoothedGain_[bin];
        const Complex mixed = original * (static_cast<T>(1) - mix_) + reduced * mix_;
        const Complex output = mixed * outputGain_;

        outputSpectrum_[bin] = magnitude(output);
        reductionCurve_[bin] = smoothedGain_[bin];
        reductionDbSum += gainToDb(smoothedGain_[bin]);
        return output;
    }

    void mirrorConjugateBin(Complex* spectrum, std::size_t bin, const Complex& value) const noexcept
    {
        if (bin == 0 || bin == FFTSize / 2)
            return;

        spectrum[FFTSize - bin] = std::conj(value);
    }

    [[nodiscard]] T presenceReductionScale(std::size_t bin) const noexcept
    {
        if (presenceProtect_ <= static_cast<T>(0) || sampleRate_ <= static_cast<T>(0))
            return static_cast<T>(1);

        const T frequency = static_cast<T>(bin) * sampleRate_ / static_cast<T>(FFTSize);
        T protectionWeight = static_cast<T>(0);

        if (frequency >= static_cast<T>(1500) && frequency <= static_cast<T>(6000))
        {
            protectionWeight = static_cast<T>(1);
        }
        else if (frequency >= static_cast<T>(800) && frequency < static_cast<T>(1500))
        {
            protectionWeight = (frequency - static_cast<T>(800)) / static_cast<T>(700);
        }
        else if (frequency > static_cast<T>(6000) && frequency <= static_cast<T>(10000))
        {
            protectionWeight = (static_cast<T>(10000) - frequency) / static_cast<T>(4000);
        }

        protectionWeight = clamp01(protectionWeight);
        const T protectionDepth = presenceProtect_ * static_cast<T>(0.65);
        return std::clamp(static_cast<T>(1) - protectionDepth * protectionWeight,
                          static_cast<T>(0.25),
                          static_cast<T>(1));
    }

    T sampleRate_ { static_cast<T>(44100) };
    bool learnNoiseEnabled_ { false };
    bool subtractNoiseEnabled_ { false };
    bool profileReady_ { false };

    std::size_t learnedFrameCount_ { 0 };
    std::size_t minimumLearnFrames_ { 8 };

    T outputGainDb_ { static_cast<T>(0) };
    T outputGain_ { static_cast<T>(1) };
    T presenceProtect_ { static_cast<T>(0.5) };
    T reductionAmount_ { static_cast<T>(0.6) };
    T spectralFloorDb_ { static_cast<T>(-60) };
    T spectralFloorGain_ { static_cast<T>(0.001) };
    T maxReductionDb_ { static_cast<T>(24) };
    T maxReductionGain_ { static_cast<T>(0.06309573444801933) };
    T smoothing_ { static_cast<T>(0.637) };
    T transientProtection_ { static_cast<T>(0.25) };
    std::size_t frequencySmoothingBins_ { 2 };
    T mix_ { static_cast<T>(1) };
    T averageReductionDb_ { static_cast<T>(0) };
    T synthesisGain_ { static_cast<T>(1) };
    bool prepared_ { false };

    FFT<T> fft_ {};
    typename WindowFunctions<T, FFTSize>::WindowBuffer window_ {};
    std::array<T, FFTSize> inputRing_ {};
    std::array<T, kOutputBufferSize> outputRing_ {};
    std::array<T, FFTSize> timeFrame_ {};
    std::array<Complex, FFTSize> spectrumFrame_ {};
    std::size_t inputWriteIndex_ { 0 };
    std::size_t outputReadIndex_ { 0 };
    std::size_t hopCounter_ { 0 };

    SpectrumView noiseProfileSum_ {};
    SpectrumView noiseProfile_ {};
    SpectrumView inputSpectrum_ {};
    SpectrumView outputSpectrum_ {};
    SpectrumView reductionCurve_ {};
    SpectrumView smoothedGain_ {};
    SpectrumView targetGain_ {};
    SpectrumView frequencySmoothedGain_ {};
};

using SpectralNoiseReducer512F = SpectralNoiseReducer<float, 512>;
using SpectralNoiseReducer1024F = SpectralNoiseReducer<float, 1024>;
using SpectralNoiseReducer2048F = SpectralNoiseReducer<float, 2048>;
using SpectralNoiseReducer4096F = SpectralNoiseReducer<float, 4096>;
using SpectralNoiseReducer8192F = SpectralNoiseReducer<float, 8192>;

using SpectralNoiseReducer512D = SpectralNoiseReducer<double, 512>;
using SpectralNoiseReducer1024D = SpectralNoiseReducer<double, 1024>;
using SpectralNoiseReducer2048D = SpectralNoiseReducer<double, 2048>;
using SpectralNoiseReducer4096D = SpectralNoiseReducer<double, 4096>;
using SpectralNoiseReducer8192D = SpectralNoiseReducer<double, 8192>;

} // namespace cvdsp::spectral

namespace cvdsp
{
using SpectralNoiseReducerState = spectral::SpectralNoiseReducerState;

template<typename T = float, std::size_t FFTSize = 2048, std::size_t OverlapPercent = 75>
using SpectralNoiseReducer = spectral::SpectralNoiseReducer<T, FFTSize, OverlapPercent>;

using SpectralNoiseReducer512F = spectral::SpectralNoiseReducer512F;
using SpectralNoiseReducer1024F = spectral::SpectralNoiseReducer1024F;
using SpectralNoiseReducer2048F = spectral::SpectralNoiseReducer2048F;
using SpectralNoiseReducer4096F = spectral::SpectralNoiseReducer4096F;
using SpectralNoiseReducer8192F = spectral::SpectralNoiseReducer8192F;
using SpectralNoiseReducer512D = spectral::SpectralNoiseReducer512D;
using SpectralNoiseReducer1024D = spectral::SpectralNoiseReducer1024D;
using SpectralNoiseReducer2048D = spectral::SpectralNoiseReducer2048D;
using SpectralNoiseReducer4096D = spectral::SpectralNoiseReducer4096D;
using SpectralNoiseReducer8192D = spectral::SpectralNoiseReducer8192D;
} // namespace cvdsp

#endif // CVDSP_SPECTRAL_SPECTRALNOISEREDUCER_HPP
