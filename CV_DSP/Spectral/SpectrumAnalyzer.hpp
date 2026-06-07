#ifndef CVDSP_SPECTRAL_SPECTRUMANALYZER_HPP
#define CVDSP_SPECTRAL_SPECTRUMANALYZER_HPP

/**
 * @file SpectrumAnalyzer.hpp
 * @brief FFT Spectrum Analyzer
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 * - FFT.hpp
 * - WindowFunctions.hpp
 * - STFT.hpp
 */

#include <array>
#include <complex>
#include <cmath>
#include <cstddef>
#include <type_traits>

#include "FFT.hpp"
#include "WindowFunctions.hpp"
#include "STFT.hpp"

namespace cvdsp
{

template<
    typename T,
    std::size_t FFTSize>
class SpectrumAnalyzer
{
    static_assert(
        std::is_floating_point_v<T>,
        "SpectrumAnalyzer requires floating point type");

public:

    static constexpr std::size_t NumBins =
        FFTSize / 2;

    using Complex =
        std::complex<T>;

    using MagnitudeArray =
        std::array<T, NumBins>;

public:

    SpectrumAnalyzer() = default;

    bool prepare(
        WindowType windowType =
            WindowType::BlackmanHarris,
        T peakHoldReleaseDbPerFrame =
            static_cast<T>(0.25))
        noexcept
    {
        peakDecay_ =
            peakHoldReleaseDbPerFrame;

        if (!fft_.prepare(
                FFTSize))
        {
            return false;
        }

        window_ =
            WindowFunctions<
                T,
                FFTSize>::generate(
                    windowType);

        reset();

        return true;
    }

    void reset() noexcept
    {
        for (auto& v : inputFrame_)
            v = T(0);

        for (auto& v : spectrum_)
            v = Complex(T(0), T(0));

        for (auto& v : magnitudes_)
            v = T(0);

        for (auto& v : rmsSpectrum_)
            v = T(0);

        for (auto& v : peakSpectrum_)
            v = T(0);
    }

    /**
     * Process one FFT frame.
     *
     * Input size must be FFTSize.
     */
    void process(
        const T* input) noexcept
    {
        copyInput(input);

        WindowFunctions<
            T,
            FFTSize>::apply(
                inputFrame_.data(),
                window_);

        buildComplexBuffer();

        fft_.forward(
            spectrum_.data());

        computeMagnitudeSpectrum();

        computeRMSSpectrum();

        updatePeakHold();
    }

    /**
     * Linear magnitudes.
     */
    [[nodiscard]]
    const MagnitudeArray&
    getMagnitudes() const noexcept
    {
        return magnitudes_;
    }

    /**
     * RMS spectrum.
     */
    [[nodiscard]]
    const MagnitudeArray&
    getRMSSpectrum() const noexcept
    {
        return rmsSpectrum_;
    }

    /**
     * Peak hold spectrum.
     */
    [[nodiscard]]
    const MagnitudeArray&
    getPeakSpectrum() const noexcept
    {
        return peakSpectrum_;
    }

    /**
     * Magnitude in dBFS.
     */
    [[nodiscard]]
    T getMagnitudeDB(
        std::size_t bin) const noexcept
    {
        if (bin >= NumBins)
        {
            return static_cast<T>(-120);
        }

        return linearToDB(
            magnitudes_[bin]);
    }

    /**
     * Peak Hold in dBFS.
     */
    [[nodiscard]]
    T getPeakDB(
        std::size_t bin) const noexcept
    {
        if (bin >= NumBins)
        {
            return static_cast<T>(-120);
        }

        return linearToDB(
            peakSpectrum_[bin]);
    }

    /**
     * RMS in dBFS.
     */
    [[nodiscard]]
    T getRMSDB(
        std::size_t bin) const noexcept
    {
        if (bin >= NumBins)
        {
            return static_cast<T>(-120);
        }

        return linearToDB(
            rmsSpectrum_[bin]);
    }

    /**
     * Bin frequency.
     */
    [[nodiscard]]
    static constexpr T
    binFrequency(
        std::size_t bin,
        T sampleRate) noexcept
    {
        return
            static_cast<T>(bin)
            *
            sampleRate
            /
            static_cast<T>(FFTSize);
    }

private:

    void copyInput(
        const T* input) noexcept
    {
        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            inputFrame_[i] =
                input[i];
        }
    }

    void buildComplexBuffer()
        noexcept
    {
        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            spectrum_[i] =
                Complex(
                    inputFrame_[i],
                    T(0));
        }
    }

    void computeMagnitudeSpectrum()
        noexcept
    {
        const T scale =
            static_cast<T>(2)
            /
            static_cast<T>(FFTSize);

        for (std::size_t i = 0;
             i < NumBins;
             ++i)
        {
            magnitudes_[i] =
                std::abs(
                    spectrum_[i])
                *
                scale;
        }
    }

    void computeRMSSpectrum()
        noexcept
    {
        constexpr T alpha =
            static_cast<T>(0.15);

        for (std::size_t i = 0;
             i < NumBins;
             ++i)
        {
            const T power =
                magnitudes_[i]
                *
                magnitudes_[i];

            rmsSpectrum_[i] =
                std::sqrt(
                    alpha * power
                    +
                    (static_cast<T>(1) - alpha)
                    *
                    rmsSpectrum_[i]
                    *
                    rmsSpectrum_[i]);
        }
    }

    void updatePeakHold()
        noexcept
    {
        const T decay =
            dbToLinear(
                -peakDecay_);

        for (std::size_t i = 0;
             i < NumBins;
             ++i)
        {
            if (magnitudes_[i] >
                peakSpectrum_[i])
            {
                peakSpectrum_[i] =
                    magnitudes_[i];
            }
            else
            {
                peakSpectrum_[i] *=
                    decay;
            }
        }
    }

    [[nodiscard]]
    static T linearToDB(
        T value) noexcept
    {
        constexpr T minimum =
            static_cast<T>(1e-12);

        if (value < minimum)
        {
            value = minimum;
        }

        return
            static_cast<T>(20)
            *
            std::log10(value);
    }

    [[nodiscard]]
    static T dbToLinear(
        T db) noexcept
    {
        return
            std::pow(
                static_cast<T>(10),
                db /
                static_cast<T>(20));
    }

private:

    FFT<T> fft_;

    typename WindowFunctions<
        T,
        FFTSize>::WindowBuffer window_;

    std::array<T, FFTSize>
        inputFrame_{};

    std::array<Complex, FFTSize>
        spectrum_{};

    MagnitudeArray magnitudes_{};

    MagnitudeArray peakSpectrum_{};

    MagnitudeArray rmsSpectrum_{};

    T peakDecay_ =
        static_cast<T>(0.25);
};

using SpectrumAnalyzer512F =
    SpectrumAnalyzer<float, 512>;

using SpectrumAnalyzer1024F =
    SpectrumAnalyzer<float, 1024>;

using SpectrumAnalyzer2048F =
    SpectrumAnalyzer<float, 2048>;

using SpectrumAnalyzer4096F =
    SpectrumAnalyzer<float, 4096>;

using SpectrumAnalyzer8192F =
    SpectrumAnalyzer<float, 8192>;

using SpectrumAnalyzer512D =
    SpectrumAnalyzer<double, 512>;

using SpectrumAnalyzer1024D =
    SpectrumAnalyzer<double, 1024>;

using SpectrumAnalyzer2048D =
    SpectrumAnalyzer<double, 2048>;

using SpectrumAnalyzer4096D =
    SpectrumAnalyzer<double, 4096>;

using SpectrumAnalyzer8192D =
    SpectrumAnalyzer<double, 8192>;

} // namespace cvdsp

#endif
