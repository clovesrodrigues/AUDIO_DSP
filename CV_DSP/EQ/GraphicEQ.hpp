#ifndef CVDSP_EQ_GRAPHIC_EQ_HPP
#define CVDSP_EQ_GRAPHIC_EQ_HPP

/**
 * @file GraphicEQ.hpp
 * @brief Header-only fixed-band graphic equalizer built from peaking biquads.
 *
 * @namespace cvdsp::eq
 *
 * @section overview Overview
 * `GraphicEQ<T, NumBands>` is a deterministic graphic EQ. Every band is a
 * `cvdsp::filters::Biquad<T>` configured as `PeakingEQ`, and bands are applied
 * in cascade:
 *
 * @f[
 *   y[n] = \prod_{k=0}^{N-1} H_k(z)\,x[n]
 * @f]
 *
 * Standard center frequencies are embedded for common 10-band octave, 15-band
 * two-thirds-octave, and 31-band one-third-octave layouts. Other band counts
 * use deterministic logarithmic spacing between 20 Hz and 20 kHz during
 * `prepare()`.
 *
 * @section rt Real-time guarantees
 * - No dynamic memory is used.
 * - `process()` is O(NumBands), `noexcept`, and lock-free.
 * - `processBlock()` is O(numSamples * NumBands), `noexcept`, and lock-free.
 * - `setBandGainDB()` recalculates one biquad and should be called from a
 *   control/block boundary, optionally after host-parameter smoothing.
 */

#include <cmath>
#include <cstddef>
#include <type_traits>

#include "../Core/Namespace.hpp"
#include "../Core/Types.hpp"
#include "../Filters/Biquad.hpp"

namespace cvdsp::eq
{

/**
 * @class GraphicEQ
 * @brief Fixed-band graphic EQ using one peaking biquad per band.
 *
 * @tparam T Floating-point DSP scalar (`cvdsp::f32` or `cvdsp::f64`).
 * @tparam NumBands Number of peaking bands in the graphic EQ.
 */
template <typename T, std::size_t NumBands>
class GraphicEQ
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "GraphicEQ requires a floating point type");

    using value_type = T;
    using size_type = std::size_t;

    /**
     * @brief Constructs a flat graphic EQ.
     */
    constexpr GraphicEQ() noexcept = default;

    /**
     * @brief Prepares all peaking bands and assigns center frequencies.
     *
     * Frequencies are selected from built-in ISO-style layouts for 10, 15, and
     * 31 bands. For other band counts, log spacing is computed between 20 Hz and
     * 20 kHz. This work is not part of the sample hot path.
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);

        for (size_type i = 0; i < NumBands; ++i)
        {
            m_centerFrequencies[i] = defaultCenterFrequency(i);
            m_bands[i].prepare(m_sampleRate);
            configureBand(i);
            m_bands[i].reset();
        }
    }

    /**
     * @brief Resets all filter states while preserving gains/frequencies.
     */
    CVDSP_FORCE_INLINE void reset() noexcept
    {
        for (size_type i = 0; i < NumBands; ++i)
            m_bands[i].reset();
    }

    /**
     * @brief Sets the gain of one graphic EQ band in dB.
     *
     * @return `true` when @p index is valid, otherwise `false`.
     */
    CVDSP_NODISCARD inline bool setBandGainDB(size_type index,
                                              value_type gainDB) noexcept
    {
        if (index >= NumBands)
            return false;

        m_gainsDB[index] = gainDB;
        configureBand(index);
        return true;
    }

    /**
     * @brief Returns a band's gain in dB.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE value_type getBandGainDB(size_type index) const noexcept
    {
        return index < NumBands ? m_gainsDB[index] : static_cast<value_type>(0);
    }

    /**
     * @brief Returns a band's center frequency in Hz.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE value_type getBandFrequency(size_type index) const noexcept
    {
        return index < NumBands ? m_centerFrequencies[index] : static_cast<value_type>(0);
    }

    /**
     * @brief Returns the compile-time number of graphic bands.
     */
    CVDSP_NODISCARD static constexpr size_type numBands() noexcept
    {
        return NumBands;
    }

    /**
     * @brief Returns the configured sample rate.
     */
    CVDSP_NODISCARD constexpr value_type getSampleRate() const noexcept
    {
        return m_sampleRate;
    }

    /**
     * @brief Processes one sample through all peaking bands in series.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE value_type process(value_type input) noexcept
    {
        value_type y = input;
        for (size_type i = 0; i < NumBands; ++i)
            y = m_bands[i].process(y);
        return y;
    }

    /**
     * @brief Processes a mono block in-place.
     */
    inline void processBlock(value_type* buffer, size_type numSamples) noexcept
    {
        if (!buffer)
            return;

        for (size_type n = 0; n < numSamples; ++n)
            buffer[n] = process(buffer[n]);
    }

    /**
     * @brief Processes a mono block from input to output.
     */
    inline void processBlock(const value_type* input,
                             value_type* output,
                             size_type numSamples) noexcept
    {
        if (!input || !output)
            return;

        for (size_type n = 0; n < numSamples; ++n)
            output[n] = process(input[n]);
    }

private:
    static constexpr value_type kDefaultQ = static_cast<value_type>(1.41421356237309504880);

    CVDSP_NODISCARD static constexpr value_type table10(size_type index) noexcept
    {
        constexpr value_type values[10]{
            static_cast<value_type>(31.25), static_cast<value_type>(62.5),
            static_cast<value_type>(125), static_cast<value_type>(250),
            static_cast<value_type>(500), static_cast<value_type>(1000),
            static_cast<value_type>(2000), static_cast<value_type>(4000),
            static_cast<value_type>(8000), static_cast<value_type>(16000)};
        return values[index];
    }

    CVDSP_NODISCARD static constexpr value_type table15(size_type index) noexcept
    {
        constexpr value_type values[15]{
            static_cast<value_type>(25), static_cast<value_type>(40),
            static_cast<value_type>(63), static_cast<value_type>(100),
            static_cast<value_type>(160), static_cast<value_type>(250),
            static_cast<value_type>(400), static_cast<value_type>(630),
            static_cast<value_type>(1000), static_cast<value_type>(1600),
            static_cast<value_type>(2500), static_cast<value_type>(4000),
            static_cast<value_type>(6300), static_cast<value_type>(10000),
            static_cast<value_type>(16000)};
        return values[index];
    }

    CVDSP_NODISCARD static constexpr value_type table31(size_type index) noexcept
    {
        constexpr value_type values[31]{
            static_cast<value_type>(20), static_cast<value_type>(25),
            static_cast<value_type>(31.5), static_cast<value_type>(40),
            static_cast<value_type>(50), static_cast<value_type>(63),
            static_cast<value_type>(80), static_cast<value_type>(100),
            static_cast<value_type>(125), static_cast<value_type>(160),
            static_cast<value_type>(200), static_cast<value_type>(250),
            static_cast<value_type>(315), static_cast<value_type>(400),
            static_cast<value_type>(500), static_cast<value_type>(630),
            static_cast<value_type>(800), static_cast<value_type>(1000),
            static_cast<value_type>(1250), static_cast<value_type>(1600),
            static_cast<value_type>(2000), static_cast<value_type>(2500),
            static_cast<value_type>(3150), static_cast<value_type>(4000),
            static_cast<value_type>(5000), static_cast<value_type>(6300),
            static_cast<value_type>(8000), static_cast<value_type>(10000),
            static_cast<value_type>(12500), static_cast<value_type>(16000),
            static_cast<value_type>(20000)};
        return values[index];
    }

    CVDSP_NODISCARD value_type defaultCenterFrequency(size_type index) const noexcept
    {
        if constexpr (NumBands == 10)
        {
            return table10(index);
        }
        else if constexpr (NumBands == 15)
        {
            return table15(index);
        }
        else if constexpr (NumBands == 31)
        {
            return table31(index);
        }
        else
        {
            if constexpr (NumBands <= 1)
                return static_cast<value_type>(1000);
            else
            {
                const value_type minHz = static_cast<value_type>(20);
                const value_type maxHz = static_cast<value_type>(20000);
                const value_type t = static_cast<value_type>(index) /
                                     static_cast<value_type>(NumBands - 1);
                return minHz * std::pow(maxHz / minHz, t);
            }
        }
    }

    inline void configureBand(size_type index) noexcept
    {
        auto& band = m_bands[index];
        band.setType(cvdsp::filters::BiquadType::PeakingEQ);
        band.setFrequency(m_centerFrequencies[index]);
        band.setQ(kDefaultQ);
        band.setGainDB(m_gainsDB[index]);
        band.updateCoefficients();
    }

    cvdsp::filters::Biquad<value_type> m_bands[NumBands > 0 ? NumBands : 1]{};
    value_type m_centerFrequencies[NumBands > 0 ? NumBands : 1]{};
    value_type m_gainsDB[NumBands > 0 ? NumBands : 1]{};
    value_type m_sampleRate{static_cast<value_type>(48000)};
};

} // namespace cvdsp::eq

namespace cvdsp
{
using eq::GraphicEQ;
} // namespace cvdsp

#endif // CVDSP_EQ_GRAPHIC_EQ_HPP
