#ifndef CVDSP_SPECTRAL_FFT_HPP
#define CVDSP_SPECTRAL_FFT_HPP

/**
 * @file FFT.hpp
 * @brief Iterative Cooley-Tukey FFT
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - Iterative FFT
 * - Iterative IFFT
 * - Bit Reversal
 * - Radix-2
 * - std::complex
 * - No recursion
 *
 * Supported Sizes:
 * - 512
 * - 1024
 * - 2048
 * - 4096
 * - 8192
 *
 * Dependencies:
 * - Core/Constants.hpp
 * - Math/FastMath.hpp
 */

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cmath>

#include "../Core/Constants.hpp"
#include "../Math/FastMath.hpp"

namespace cvdsp
{

template<typename T>
class FFT
{
    static_assert(
        std::is_floating_point_v<T>,
        "FFT requires floating point type");

public:

    using Complex = std::complex<T>;

    static constexpr std::size_t MaxFFTSize = 8192;

public:

    constexpr FFT() noexcept = default;

    /**
     * @brief Prepare FFT instance.
     *
     * Allowed:
     *
     * 512
     * 1024
     * 2048
     * 4096
     * 8192
     */
    bool prepare(
        std::size_t fftSize) noexcept
    {
        if (
            fftSize != 512  &&
            fftSize != 1024 &&
            fftSize != 2048 &&
            fftSize != 4096 &&
            fftSize != 8192)
        {
            return false;
        }

        fftSize_ = fftSize;

        buildBitReverseTable();
        buildTwiddleTable();

        prepared_ = true;

        return true;
    }

    /**
     * @brief Reset state.
     */
    void reset() noexcept
    {
        fftSize_ = 0;

        prepared_ = false;

        bitReverse_.fill(0);
        twiddles_.fill(
            Complex(
                static_cast<T>(0),
                static_cast<T>(0)));
    }

    /**
     * @brief Forward FFT.
     *
     * Input and output may be
     * the same buffer.
     */
    void forward(
        Complex* data) const noexcept
    {
        if (!prepared_)
        {
            return;
        }

        bitReversePermutation(
            data);

        iterativeFFT(
            data,
            false);
    }

    /**
     * @brief Inverse FFT.
     */
    void inverse(
        Complex* data) const noexcept
    {
        if (!prepared_)
        {
            return;
        }

        bitReversePermutation(
            data);

        iterativeFFT(
            data,
            true);

        const T scale =
            static_cast<T>(1)
            /
            static_cast<T>(fftSize_);

        for (std::size_t i = 0;
             i < fftSize_;
             ++i)
        {
            data[i] *= scale;
        }
    }

    [[nodiscard]]
    constexpr std::size_t getFFTSize() const noexcept
    {
        return fftSize_;
    }

private:

    void buildBitReverseTable() noexcept
    {
        const std::size_t bits =
            log2Int(fftSize_);

        for (std::size_t i = 0;
             i < fftSize_;
             ++i)
        {
            bitReverse_[i] =
                reverseBits(
                    i,
                    bits);
        }
    }

    void buildTwiddleTable() noexcept
    {
        constexpr T pi =
            static_cast<T>(
                3.1415926535897932384626433832795);

        for (std::size_t k = 0;
             k < fftSize_ / 2;
             ++k)
        {
            const T angle =
                static_cast<T>(-2)
                *
                pi
                *
                static_cast<T>(k)
                /
                static_cast<T>(fftSize_);

            twiddles_[k] =
                Complex(
                    std::cos(angle),
                    std::sin(angle));
        }
    }

    void bitReversePermutation(
        Complex* data) const noexcept
    {
        for (std::size_t i = 0;
             i < fftSize_;
             ++i)
        {
            const std::size_t j =
                bitReverse_[i];

            if (j > i)
            {
                const Complex temp =
                    data[i];

                data[i] =
                    data[j];

                data[j] =
                    temp;
            }
        }
    }

    void iterativeFFT(
        Complex* data,
        bool inverse) const noexcept
    {
        for (
            std::size_t stage = 2;
            stage <= fftSize_;
            stage <<= 1)
        {
            const std::size_t half =
                stage >> 1;

            const std::size_t twiddleStep =
                fftSize_
                /
                stage;

            for (
                std::size_t start = 0;
                start < fftSize_;
                start += stage)
            {
                for (
                    std::size_t k = 0;
                    k < half;
                    ++k)
                {
                    Complex twiddle =
                        twiddles_[
                            k * twiddleStep];

                    if (inverse)
                    {
                        twiddle =
                            std::conj(
                                twiddle);
                    }

                    const Complex even =
                        data[start + k];

                    const Complex odd =
                        twiddle
                        *
                        data[start + k + half];

                    data[start + k] =
                        even + odd;

                    data[start + k + half] =
                        even - odd;
                }
            }
        }
    }

    [[nodiscard]]
    static constexpr std::size_t
    log2Int(
        std::size_t value) noexcept
    {
        std::size_t result = 0;

        while (value > 1)
        {
            value >>= 1;
            ++result;
        }

        return result;
    }

    [[nodiscard]]
    static constexpr std::size_t
    reverseBits(
        std::size_t value,
        std::size_t bits) noexcept
    {
        std::size_t result = 0;

        for (std::size_t i = 0;
             i < bits;
             ++i)
        {
            result <<= 1;

            result |=
                (value & 1);

            value >>= 1;
        }

        return result;
    }

private:

    bool prepared_ = false;

    std::size_t fftSize_ = 0;

    std::array<
        std::size_t,
        MaxFFTSize> bitReverse_{};

    std::array<
        Complex,
        MaxFFTSize / 2> twiddles_{};
};

using FFTf = FFT<float>;
using FFTd = FFT<double>;

} // namespace cvdsp

#endif
