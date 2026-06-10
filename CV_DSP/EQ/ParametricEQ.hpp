#ifndef CVDSP_EQ_PARAMETRIC_EQ_HPP
#define CVDSP_EQ_PARAMETRIC_EQ_HPP

/**
 * @file ParametricEQ.hpp
 * @brief Header-only parametric equalizer built from cascaded CV_DSP biquads.
 *
 * @namespace cvdsp::eq
 *
 * @section overview Overview
 * `ParametricEQ<T, MaxBands>` implements a fixed-capacity parametric EQ as a
 * cascade of second-order IIR sections:
 *
 * @f[
 *   y[n] = H_M(z)\,H_{M-1}(z)\,\dots\,H_1(z)\,x[n]
 * @f]
 *
 * where each @f$H_i(z)@f$ is a `cvdsp::filters::Biquad<T>`. No VST3 SDK types
 * are used. All storage is embedded in the object and the audio hot path does
 * not allocate, lock, throw, or use RTTI.
 *
 * @section rt Real-time guarantees
 * - `process()` is O(MaxBands), `noexcept`, and allocation-free.
 * - `processBlock()` is O(numSamples * MaxBands), `noexcept`, and
 *   allocation-free.
 * - Coefficient updates use the existing `Biquad<T>` implementation and should
 *   be driven from the control/block boundary, optionally after smoothing via
 *   `CV_DSP/Manager/ParameterManager.hpp` or `ParameterSmoother.hpp`.
 */

#include <cstddef>
#include <type_traits>

#include "../Core/Namespace.hpp"
#include "../Core/Types.hpp"
#include "../Filters/Biquad.hpp"

namespace cvdsp::eq
{

/**
 * @class ParametricEQ
 * @brief Fixed-capacity parametric EQ using cascaded `filters::Biquad<T>` bands.
 *
 * @tparam T Floating-point DSP scalar (`cvdsp::f32` or `cvdsp::f64`).
 * @tparam MaxBands Maximum number of embedded EQ bands.
 *
 * Each band may be independently configured as LowPass, HighPass, PeakingEQ,
 * LowShelf, HighShelf, Notch, BandPass, or AllPass through the underlying
 * `filters::BiquadType`. Inactive bands are skipped but remain allocated in the
 * object, preserving deterministic memory behavior.
 */
template <typename T, std::size_t MaxBands>
class ParametricEQ
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "ParametricEQ requires a floating point type");

    using value_type = T;
    using size_type = std::size_t;
    using FilterType = cvdsp::filters::BiquadType;

    /**
     * @brief Constructs a bypassed EQ with all bands inactive.
     */
    constexpr ParametricEQ() noexcept = default;

    /**
     * @brief Prepares every embedded biquad for a sample rate.
     *
     * Calls `prepare()` on all preallocated bands and clears their states. This
     * method performs no allocation but recalculates coefficients; call it
     * outside the per-sample hot path.
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);

        for (size_type i = 0; i < MaxBands; ++i)
            m_bands[i].prepare(m_sampleRate);
    }

    /**
     * @brief Resets filter memory for all bands while preserving coefficients.
     */
    CVDSP_FORCE_INLINE void reset() noexcept
    {
        for (size_type i = 0; i < MaxBands; ++i)
            m_bands[i].reset();
    }

    /**
     * @brief Configures one band and immediately updates its coefficients.
     *
     * @param index Band index in `[0, MaxBands)`.
     * @param type Filter topology from `cvdsp::filters::BiquadType`.
     * @param frequencyHz Cutoff/center frequency in Hz.
     * @param q Quality factor.
     * @param gainDB Gain in dB for PeakingEQ/Shelf filters.
     * @param active Whether the band participates in the cascade.
     * @return `true` when @p index is valid, otherwise `false`.
     *
     * @note For click-free automation, smooth host parameters first and call
     * this method at a safe control/block boundary. The sample hot path remains
     * coefficient-read-only.
     */
    CVDSP_NODISCARD inline bool setBand(size_type index,
                                        FilterType type,
                                        value_type frequencyHz,
                                        value_type q,
                                        value_type gainDB,
                                        bool active = true) noexcept
    {
        if (index >= MaxBands)
            return false;

        auto& band = m_bands[index];
        band.setType(type);
        band.setFrequency(frequencyHz);
        band.setQ(q);
        band.setGainDB(gainDB);
        band.updateCoefficients();
        m_active[index] = active;
        return true;
    }

    /**
     * @brief Enables or disables one preconfigured band.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool setBandActive(size_type index,
                                                          bool active) noexcept
    {
        if (index >= MaxBands)
            return false;
        m_active[index] = active;
        return true;
    }

    /**
     * @brief Returns whether a band is active.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isBandActive(size_type index) const noexcept
    {
        return index < MaxBands ? m_active[index] : false;
    }

    /**
     * @brief Returns the compile-time band capacity.
     */
    CVDSP_NODISCARD static constexpr size_type maxBands() noexcept
    {
        return MaxBands;
    }

    /**
     * @brief Returns the configured sample rate.
     */
    CVDSP_NODISCARD constexpr value_type getSampleRate() const noexcept
    {
        return m_sampleRate;
    }

    /**
     * @brief Processes one sample through all active bands in series.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE value_type process(value_type input) noexcept
    {
        value_type y = input;
        for (size_type i = 0; i < MaxBands; ++i)
        {
            if (m_active[i])
                y = m_bands[i].process(y);
        }
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
    cvdsp::filters::Biquad<value_type> m_bands[MaxBands > 0 ? MaxBands : 1]{};
    bool m_active[MaxBands > 0 ? MaxBands : 1]{};
    value_type m_sampleRate{static_cast<value_type>(48000)};
};

} // namespace cvdsp::eq

namespace cvdsp
{
using eq::ParametricEQ;
} // namespace cvdsp

#endif // CVDSP_EQ_PARAMETRIC_EQ_HPP
