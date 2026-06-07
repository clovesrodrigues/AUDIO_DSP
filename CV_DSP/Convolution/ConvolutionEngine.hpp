#ifndef CVDSP_CONVOLUTION_CONVOLUTIONENGINE_HPP
#define CVDSP_CONVOLUTION_CONVOLUTIONENGINE_HPP

/**
 * @file ConvolutionEngine.hpp
 * @brief FFT Partitioned Convolution Engine
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - FFT Convolution
 * - Partitioned IR
 * - Overlap Save
 * - Long IR Support
 *
 * No dynamic allocation in process().
 */

#include <array>
#include <complex>
#include <cstddef>
#include <type_traits>

#include "../Spectral/FFT.hpp"
#include "../Core/CircularBuffer.hpp"

namespace cvdsp
{

template<
    typename T,
    std::size_t FFTSize,
    std::size_t MaxIRSamples = 262144>
class ConvolutionEngine
{
    static_assert(
        std::is_floating_point_v<T>);

public:

    using Complex =
        std::complex<T>;

    static constexpr std::size_t BlockSize =
        FFTSize / 2;

    static constexpr std::size_t MaxPartitions =
        MaxIRSamples / BlockSize + 1;

public:

    ConvolutionEngine() = default;

    bool prepare() noexcept
    {
        if (!fft_.prepare(
                FFTSize))
        {
            return false;
        }

        reset();

        return true;
    }

    void reset() noexcept
    {
        inputPosition_ = 0;
        currentPartition_ = 0;
        numPartitions_ = 0;

        inputBuffer_.reset();

        for (auto& v : overlap_)
            v = T(0);

        for (auto& v : timeDomain_)
            v = T(0);

        for (auto& v : spectrum_)
            v = Complex(T(0), T(0));

        for (auto& p : partitionSpectra_)
        {
            for (auto& v : p)
            {
                v = Complex(T(0), T(0));
            }
        }

        for (auto& p : inputHistory_)
        {
            for (auto& v : p)
            {
                v = Complex(T(0), T(0));
            }
        }
    }

    /**
     * Load impulse response.
     */
    bool loadImpulseResponse(
        const T* ir,
        std::size_t length)
        noexcept
    {
        reset();

        if (length == 0)
        {
            return false;
        }

        numPartitions_ =
            (length + BlockSize - 1)
            /
            BlockSize;

        if (numPartitions_ >
            MaxPartitions)
        {
            return false;
        }

        std::array<T, FFTSize>
            partitionTime{};

        for (std::size_t p = 0;
             p < numPartitions_;
             ++p)
        {
            for (auto& v : partitionTime)
                v = T(0);

            const std::size_t offset =
                p * BlockSize;

            for (std::size_t i = 0;
                 i < BlockSize;
                 ++i)
            {
                if ((offset + i)
                    < length)
                {
                    partitionTime[i] =
                        ir[offset + i];
                }
            }

            for (std::size_t i = 0;
                 i < FFTSize;
                 ++i)
            {
                partitionSpectra_[p][i] =
                    Complex(
                        partitionTime[i],
                        T(0));
            }

            fft_.forward(
                partitionSpectra_[p].data());
        }

        return true;
    }

    /**
     * Process one sample.
     */
    T process(
        T input)
        noexcept
    {
        inputBuffer_.push(
            input);

        const T output =
            outputFIFO_[outputIndex_];

        outputFIFO_[outputIndex_] =
            T(0);

        ++outputIndex_;

        if (outputIndex_
            >= BlockSize)
        {
            outputIndex_ = 0;

            processBlock();
        }

        return output;
    }

private:

    void processBlock()
        noexcept
    {
        for (std::size_t i = 0;
             i < BlockSize;
             ++i)
        {
            timeDomain_[i] =
                inputBuffer_.read(
                    BlockSize - 1 - i);
        }

        for (std::size_t i = BlockSize;
             i < FFTSize;
             ++i)
        {
            timeDomain_[i] = T(0);
        }

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

        for (std::size_t i =
                numPartitions_ - 1;
             i > 0;
             --i)
        {
            inputHistory_[i] =
                inputHistory_[i - 1];
        }

        inputHistory_[0] =
            spectrum_;

        for (std::size_t i = 0;
             i < FFTSize;
             ++i)
        {
            accumulation_[i] =
                Complex(
                    T(0),
                    T(0));
        }

        for (std::size_t p = 0;
             p < numPartitions_;
             ++p)
        {
            for (std::size_t k = 0;
                 k < FFTSize;
                 ++k)
            {
                accumulation_[k] +=
                    inputHistory_[p][k]
                    *
                    partitionSpectra_[p][k];
            }
        }

        fft_.inverse(
            accumulation_.data());

        for (std::size_t i = 0;
             i < BlockSize;
             ++i)
        {
            outputFIFO_[i] =
                accumulation_[
                    i + BlockSize]
                    .real();
        }
    }

private:

    FFT<T> fft_;

    CircularBuffer<
        T,
        FFTSize * 4>
        inputBuffer_;

    std::array<T, FFTSize>
        timeDomain_{};

    std::array<T, BlockSize>
        overlap_{};

    std::array<T, BlockSize>
        outputFIFO_{};

    std::array<Complex, FFTSize>
        spectrum_{};

    std::array<Complex, FFTSize>
        accumulation_{};

    std::array<
        std::array<
            Complex,
            FFTSize>,
        MaxPartitions>
        partitionSpectra_{};

    std::array<
        std::array<
            Complex,
            FFTSize>,
        MaxPartitions>
        inputHistory_{};

    std::size_t numPartitions_ = 0;

    std::size_t inputPosition_ = 0;

    std::size_t outputIndex_ = 0;

    std::size_t currentPartition_ = 0;
};

using Convolution2048F =
    ConvolutionEngine<
        float,
        2048>;

using Convolution4096F =
    ConvolutionEngine<
        float,
        4096>;

using Convolution8192F =
    ConvolutionEngine<
        float,
        8192>;

} // namespace cvdsp

#endif
