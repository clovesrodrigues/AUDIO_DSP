#ifndef CVDSP_SATURATION_TAPESATURATION_HPP
#define CVDSP_SATURATION_TAPESATURATION_HPP

/**
 * @file TapeSaturation.hpp
 * @brief Tape Saturation Processor
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - Drive
 * - Bias
 * - Compression
 * - Soft Limiting
 * - Magnetic Saturation
 *
 * Inspired by analog tape machines.
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace cvdsp::saturation
{

/**
 * @brief Tape saturation processor.
 *
 * Simulates:
 *
 * - Magnetic tape saturation
 * - Soft compression
 * - Soft limiting
 * - Harmonic enrichment
 *
 * Designed for:
 *
 * - Mix bus
 * - Master bus
 * - Tape coloration
 * - Instrument processing
 */
template<typename T>
class TapeSaturation
{
    static_assert(
        std::is_floating_point_v<T>,
        "TapeSaturation requires floating point type");

public:

    constexpr TapeSaturation() noexcept = default;

    /**
     * @brief Initialize processor.
     *
     * Present for API consistency.
     */
    void prepare(
        T sampleRate) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        reset();
    }

    /**
     * @brief Reset internal state.
     */
    void reset() noexcept
    {
        envelope_ =
            static_cast<T>(0);

        gainReduction_ =
            static_cast<T>(1);
    }

    /**
     * @brief Set input drive.
     *
     * Range:
     *
     * 1.0 .. 30.0
     */
    void setDrive(
        T drive) noexcept
    {
        drive_ =
            std::clamp(
                drive,
                static_cast<T>(1),
                static_cast<T>(30));
    }

    /**
     * @brief Set tape bias.
     *
     * Range:
     *
     * -1.0 .. +1.0
     */
    void setBias(
        T bias) noexcept
    {
        bias_ =
            std::clamp(
                bias,
                static_cast<T>(-1),
                static_cast<T>(1));
    }

    /**
     * @brief Set compression amount.
     *
     * Range:
     *
     * 0.0 .. 1.0
     */
    void setCompression(
        T compression) noexcept
    {
        compression_ =
            std::clamp(
                compression,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Process one sample.
     */
    inline T process(
        T input) noexcept
    {
        /**
         * Input drive.
         */
        T x =
            input
            *
            drive_;

        /**
         * Tape bias.
         */
        x +=
            bias_;

        /**
         * Envelope follower.
         *
         * Slow and smooth.
         */
        const T detector =
            std::fabs(x);

        envelope_ +=
            envelopeCoeff_
            *
            (
                detector
                -
                envelope_
            );

        /**
         * Tape compression.
         */
        const T compressionAmount =
            static_cast<T>(1)
            +
            (
                envelope_
                *
                compression_
                *
                static_cast<T>(4)
            );

        gainReduction_ =
            static_cast<T>(1)
            /
            compressionAmount;

        x *= gainReduction_;

        /**
         * Magnetic transfer curve.
         *
         * Smooth saturation.
         */
        T y =
            magneticCurve(x);

        /**
         * Remove part of bias.
         */
        y -=
            bias_
            *
            static_cast<T>(0.35);

        /**
         * Tape output normalization.
         */
        y *=
            outputTrim_;

        return y;
    }

private:

    /**
     * @brief Magnetic tape transfer curve.
     *
     * Softer than tube clipping.
     */
    static inline T magneticCurve(
        T x) noexcept
    {
        return
            std::tanh(
                x
                *
                static_cast<T>(0.85));
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T drive_ =
        static_cast<T>(1);

    T bias_ =
        static_cast<T>(0);

    T compression_ =
        static_cast<T>(0.5);

    /**
     * Internal compressor state.
     */
    T envelope_ =
        static_cast<T>(0);

    T gainReduction_ =
        static_cast<T>(1);

    /**
     * Fixed tape-style smoothing.
     *
     * Produces slow gain adaptation
     * similar to magnetic saturation.
     */
    T envelopeCoeff_ =
        static_cast<T>(0.001);

    /**
     * Output compensation.
     */
    T outputTrim_ =
        static_cast<T>(1.15);
};

} // namespace cvdsp::saturation

namespace cvdsp
{
template<typename T>
using TapeSaturation = saturation::TapeSaturation<T>;
} // namespace cvdsp

#endif
