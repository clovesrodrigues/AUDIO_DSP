#ifndef CVDSP_GUITAR_CABINETSIMULATOR_HPP
#define CVDSP_GUITAR_CABINETSIMULATOR_HPP

/**
 * @file CabinetSimulator.hpp
 * @brief Guitar Cabinet Simulator
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 * - ../Convolution/IRLoader.hpp
 * - ../Convolution/ConvolutionEngine.hpp
 *
 * Features:
 * - Cabinet IR Loading
 * - Mono Cabinet Processing
 * - FFT Convolution
 * - Speaker Simulation
 * - Microphone Simulation
 *
 * No dynamic allocation inside process().
 */

#include <cstddef>
#include <type_traits>

#include "../Convolution/IRLoader.hpp"
#include "../Convolution/ConvolutionEngine.hpp"

namespace cvdsp
{

template<
    typename T,
    std::size_t FFTSize = 2048,
    std::size_t MaxIRSamples = 262144>
class CabinetSimulator
{
    static_assert(
        std::is_floating_point_v<T>,
        "CabinetSimulator requires floating point type");

public:

    CabinetSimulator() = default;

    /**
     * @brief Prepare convolution engine.
     */
    bool prepare() noexcept
    {
        cabinetLoaded_ = false;

        return convolution_.prepare();
    }

    /**
     * @brief Reset processing state.
     */
    void reset() noexcept
    {
        convolution_.reset();
    }

    /**
     * @brief Load cabinet IR from WAV.
     *
     * Mono IR:
     * channel 0
     *
     * Stereo IR:
     * left channel used
     */
    bool loadCabinet(
        const std::string& filePath,
        bool normalize = true,
        bool trimSilence = true) noexcept
    {
        cabinetLoaded_ = false;

        if (!loader_.load(
                filePath,
                normalize,
                trimSilence))
        {
            return false;
        }

        const T* ir =
            loader_.getChannel(0);

        const std::size_t length =
            loader_.getLength();

        if (!convolution_.loadImpulseResponse(
                ir,
                length))
        {
            return false;
        }

        cabinetLoaded_ = true;

        return true;
    }

    /**
     * @brief Unload current cabinet.
     */
    void unloadCabinet() noexcept
    {
        loader_.unload();

        convolution_.reset();

        cabinetLoaded_ = false;
    }

    /**
     * @brief Process one sample.
     */
    T process(
        T input) noexcept
    {
        if (!cabinetLoaded_)
        {
            return input;
        }

        return convolution_.process(
            input);
    }

    /**
     * @brief Cabinet loaded?
     */
    [[nodiscard]]
    bool isLoaded() const noexcept
    {
        return cabinetLoaded_;
    }

    /**
     * @brief Cabinet IR length.
     */
    [[nodiscard]]
    std::size_t getIRLength()
        const noexcept
    {
        return loader_.getLength();
    }

    /**
     * @brief Cabinet IR sample rate.
     */
    [[nodiscard]]
    std::uint32_t getIRSampleRate()
        const noexcept
    {
        return loader_.getSampleRate();
    }

    /**
     * @brief Access IR loader.
     */
    [[nodiscard]]
    const IRLoader<
        T,
        MaxIRSamples>&
    getIRLoader() const noexcept
    {
        return loader_;
    }

private:

    IRLoader<
        T,
        MaxIRSamples>
        loader_;

    ConvolutionEngine<
        T,
        FFTSize,
        MaxIRSamples>
        convolution_;

    bool cabinetLoaded_ = false;
};

/**
 * Common aliases.
 */

using CabinetSimulator2048F =
    CabinetSimulator<
        float,
        2048>;

using CabinetSimulator4096F =
    CabinetSimulator<
        float,
        4096>;

using CabinetSimulator8192F =
    CabinetSimulator<
        float,
        8192>;

using CabinetSimulator2048D =
    CabinetSimulator<
        double,
        2048>;

using CabinetSimulator4096D =
    CabinetSimulator<
        double,
        4096>;

using CabinetSimulator8192D =
    CabinetSimulator<
        double,
        8192>;

} // namespace cvdsp

#endif
