#ifndef CVDSP_SPECTRAL_STFT_HPP
#define CVDSP_SPECTRAL_STFT_HPP

/**
 * @file STFT.hpp
 * @brief Short Time Fourier Transform
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - FFT.hpp
 * - WindowFunctions.hpp
 * - ../Core/CircularBuffer.hpp
 *
 * Features:
 *
 * - Forward STFT
 * - Inverse STFT
 * - Overlap Add (OLA)
 * - Overlap Save (OLS)
 *
 * Supported Overlap:
 *
 * - 25%
 * - 50%
 * - 75%
 *
 * No dynamic allocation inside process().
 */

#include <array>
#include <complex>
#include <cstddef>
#include <type_traits>

#include "FFT.hpp"
#include "WindowFunctions.hpp"
#include "../Core/CircularBuffer.hpp"

namespace cvdsp::spectral
{

enum class STFTMode
{
    OverlapAdd,
    OverlapSave
};

template<
    typename T,
    std::size_t FFTSize,
    std::size_t OverlapPercent = 50>
class STFT
{
    static_assert(
        std::is_floating_point_v<T>,
        "STFT requires floating point type");

    static_assert(
        FFTSize == 512  ||
        FFTSize == 1024 ||
        FFTSize == 2048 ||
        FFTSize == 4096 ||
        FFTSize == 8192,
        "Unsupported FFT size");

    static_assert(
        OverlapPercent == 25 ||
        OverlapPercent == 50 ||
        OverlapPercent == 75,
        "Overlap must be 25, 50 or 75");

public:

    using Complex =
        std::complex<T>;

    static constexpr std::size_t WindowSize =
        FFTSize;

    static constexpr std::size_t HopSize =
        FFTSize
        *
        (100 - OverlapPercent)
        / 100;

public:

    STFT() = default;

    /**
     * Prepare STFT.
     */
    bool prepare(
        WindowType windowType =
            WindowType::Hann,
        STFTMode mode =
            STFTMode::OverlapAdd)
        noexcept
    {
        mode_ = mode;

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

    /**
     * Reset state.
     */
    void reset() noexcept
    {
        inputBuffer_.reset();
        outputBuffer_.reset();

        frameCounter_ = 0;

        for (auto& v : timeDomain_)
            v = T(0);

        for (auto& v : overlapBuffer_)
            v = T(0);

        for (auto& v : spectrum_)
            v = Complex(T(0), T(0));
    }

    /**
     * Process one sample.
     *
     * Returns reconstructed output.
     */
    T process(
        T inputSample)
        noexcept
    {
        inputBuffer_.push(
            inputSample);

        T output =
            outputBuffer_.read(0);

        outputBuffer_.push(
            T(0));

        ++frameCounter_;

        if (frameCounter_ >= HopSize)
        {
            frameCounter_ = 0;

            if (mode_ ==
                STFTMode::OverlapAdd)
            {
                processOLA();
            }
            else
            {
                processOLS();
            }
        }

        return output;
    }

    /**
     * Direct spectrum access.
     */
    [[nodiscard]]
    Complex* getSpectrum()
        noexcept
    {
        return spectrum_.data();
    }

    /**
     * Const spectrum access.
     */
    [[nodiscard]]
    const Complex* getSpectrum() const
        noexcept
    {
        return spectrum_.data();
    }

    /**
     * FFT size.
     */
    [[nodiscard]]
    static constexpr std::size_t
    getFFTSize() noexcept
    {
        return FFTSize;
    }

    /**
     * Window size.
     */
    [[nodiscard]]
    static constexpr std::size_t
    getWindowSize() noexcept
    {
        return WindowSize;
    }

    /**
     * Hop size.
     */
    [[nodiscard]]
    static constexpr std::size_t
    getHopSize() noexcept
    {
        return HopSize;
    }

private:

    /**
     * Overlap Add.
     */
    void processOLA()
        noexcept
    {
        acquireFrame();

        applyWindow();

        performForwardFFT();

        /**
         * Spectral processing hook.
         */

        performInverseFFT();

        applyWindow();

        overlapAdd();
    }

    /**
     * Overlap Save.
     */
    void processOLS()
        noexcept
    {
        acquireFrame();

        applyWindow();

        performForwardFFT();

        /**
         * Spectral processing hook.
         */

        performInverseFFT();

        saveOverlap();
    }

    void acquireFrame()
        noexcept
    {
        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            timeDomain_[i] =
                inputBuffer_.read(
                    FFTSize - 1 - i);
        }
    }

    void applyWindow()
        noexcept
    {
        WindowFunctions<
            T,
            FFTSize>::apply(
                timeDomain_.data(),
                window_);
    }

    void performForwardFFT()
        noexcept
    {
        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            spectrum_[i] =
                Complex(
                    timeDomain_[i],
                    T(0));
        }

        fft_.forward(
            spectrum_.data());
    }

    void performInverseFFT()
        noexcept
    {
        fft_.inverse(
            spectrum_.data());

        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            timeDomain_[i] =
                spectrum_[i].real();
        }
    }

    void overlapAdd()
        noexcept
    {
        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            overlapBuffer_[i] +=
                timeDomain_[i];
        }

        for (std::size_t i = 0;
             i < HopSize;
             ++i)
        {
            outputBuffer_.push(
                overlapBuffer_[i]);
        }

        for (std::size_t i = 0;
             i < FFTSize - HopSize;
             ++i)
        {
            overlapBuffer_[i] =
                overlapBuffer_[
                    i + HopSize];
        }

        for (std::size_t i =
                FFTSize - HopSize;
             i < FFTSize;
             ++i)
        {
            overlapBuffer_[i] =
                T(0);
        }
    }

    void saveOverlap()
        noexcept
    {
        constexpr std::size_t overlap =
            FFTSize - HopSize;

        for (std::size_t i = overlap;
             i < FFTSize;
             ++i)
        {
            outputBuffer_.push(
                timeDomain_[i]);
        }
    }

private:

    FFT<T> fft_;

    STFTMode mode_ =
        STFTMode::OverlapAdd;

    typename WindowFunctions<
        T,
        FFTSize>::WindowBuffer window_;

    std::array<T, FFTSize>
        timeDomain_{};

    std::array<T, FFTSize>
        overlapBuffer_{};

    std::array<Complex, FFTSize>
        spectrum_{};

    CircularBuffer<
        T,
        FFTSize * 2>
        inputBuffer_;

    CircularBuffer<
        T,
        FFTSize * 2>
        outputBuffer_;

    std::size_t frameCounter_ = 0;
};

/**
 * Common aliases.
 */

using STFT512F =
    STFT<float, 512>;

using STFT1024F =
    STFT<float, 1024>;

using STFT2048F =
    STFT<float, 2048>;

using STFT4096F =
    STFT<float, 4096>;

using STFT8192F =
    STFT<float, 8192>;

using STFT512D =
    STFT<double, 512>;

using STFT1024D =
    STFT<double, 1024>;

using STFT2048D =
    STFT<double, 2048>;

using STFT4096D =
    STFT<double, 4096>;

using STFT8192D =
    STFT<double, 8192>;

} // namespace cvdsp::spectral

namespace cvdsp
{
using STFTMode = spectral::STFTMode;

template<typename T, std::size_t FFTSize, std::size_t OverlapPercent = 50>
using STFT = spectral::STFT<T, FFTSize, OverlapPercent>;

using STFT512F = spectral::STFT512F;
using STFT1024F = spectral::STFT1024F;
using STFT2048F = spectral::STFT2048F;
using STFT4096F = spectral::STFT4096F;
using STFT8192F = spectral::STFT8192F;
using STFT512D = spectral::STFT512D;
using STFT1024D = spectral::STFT1024D;
using STFT2048D = spectral::STFT2048D;
using STFT4096D = spectral::STFT4096D;
using STFT8192D = spectral::STFT8192D;
} // namespace cvdsp

#endif
