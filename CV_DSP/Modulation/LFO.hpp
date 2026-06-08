#ifndef CVDSP_MODULATION_LFO_HPP
#define CVDSP_MODULATION_LFO_HPP

/**
 * @file LFO.hpp
 * @brief Low Frequency Oscillator
 *
 * Header-only
 * C++20
 * Real-Time Safe
 * No dynamic allocation
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace cvdsp::modulation
{

/**
 * @brief LFO waveform types.
 */
enum class LFOWaveform
{
    Sine,
    Triangle,
    Saw,
    Square
};

/**
 * @brief Low Frequency Oscillator.
 *
 * Frequency Range:
 *
 * 0.01 Hz
 * to
 * 50 Hz
 *
 * Output Range:
 *
 * depth = 1.0
 *
 * [-1, +1]
 *
 * depth = 0.5
 *
 * [-0.5, +0.5]
 *
 * Suitable for:
 *
 * - Chorus
 * - Flanger
 * - Phaser
 * - Tremolo
 * - Auto Pan
 * - Filter Modulation
 */
template<typename T>
class LFO
{
    static_assert(
        std::is_floating_point_v<T>,
        "LFO requires floating point type");

public:

    constexpr LFO() noexcept = default;

    /**
     * @brief Prepare LFO.
     *
     * @param sampleRate Processing sample rate.
     * @param rateHz Initial rate.
     * @param depth Initial depth.
     * @param waveform Waveform.
     */
    void prepare(
        T sampleRate,
        T rateHz,
        T depth,
        LFOWaveform waveform) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        waveform_ = waveform;

        setRate(rateHz);
        setDepth(depth);

        reset();
    }

    /**
     * @brief Reset phase accumulator.
     */
    void reset() noexcept
    {
        phase_ = static_cast<T>(0);
    }

    /**
     * @brief Set LFO frequency.
     *
     * Range:
     *
     * 0.01 Hz
     * to
     * 50 Hz
     */
    void setRate(
        T rateHz) noexcept
    {
        constexpr T kMinRate =
            static_cast<T>(0.01);

        constexpr T kMaxRate =
            static_cast<T>(50.0);

        rateHz_ =
            std::clamp(
                rateHz,
                kMinRate,
                kMaxRate);

        phaseIncrement_ =
            rateHz_ / sampleRate_;
    }

    /**
     * @brief Set modulation depth.
     *
     * Range:
     *
     * 0.0 ... 1.0
     */
    void setDepth(
        T depth) noexcept
    {
        depth_ =
            std::clamp(
                depth,
                static_cast<T>(0),
                static_cast<T>(1));
    }

    /**
     * @brief Select waveform.
     */
    void setWaveform(
        LFOWaveform waveform) noexcept
    {
        waveform_ = waveform;
    }

    /**
     * @brief Current rate.
     */
    [[nodiscard]]
    T getRate() const noexcept
    {
        return rateHz_;
    }

    /**
     * @brief Current depth.
     */
    [[nodiscard]]
    T getDepth() const noexcept
    {
        return depth_;
    }

    /**
     * @brief Generate one LFO sample.
     *
     * Output:
     *
     * [-depth,+depth]
     */
    inline T process() noexcept
    {
        T value {};

        switch (waveform_)
        {
            case LFOWaveform::Sine:
            {
                value = processSine();
                break;
            }

            case LFOWaveform::Triangle:
            {
                value = processTriangle();
                break;
            }

            case LFOWaveform::Saw:
            {
                value = processSaw();
                break;
            }

            case LFOWaveform::Square:
            {
                value = processSquare();
                break;
            }

            default:
            {
                value = static_cast<T>(0);
                break;
            }
        }

        advancePhase();

        return value * depth_;
    }

private:

    /**
     * @brief Advance normalized phase.
     *
     * Range:
     *
     * [0,1)
     */
    inline void advancePhase() noexcept
    {
        phase_ += phaseIncrement_;

        if (phase_ >= static_cast<T>(1))
        {
            phase_ -= static_cast<T>(1);
        }
    }

    /**
     * @brief Sine waveform.
     */
    inline T processSine() const noexcept
    {
        constexpr T kTwoPi =
            static_cast<T>(
                6.283185307179586476925286766559);

        return std::sin(
            phase_ * kTwoPi);
    }

    /**
     * @brief Triangle waveform.
     */
    inline T processTriangle() const noexcept
    {
        return
            static_cast<T>(1)
            -
            static_cast<T>(4)
            *
            std::abs(
                phase_
                -
                static_cast<T>(0.5));
    }

    /**
     * @brief Saw waveform.
     */
    inline T processSaw() const noexcept
    {
        return
            static_cast<T>(2)
            *
            phase_
            -
            static_cast<T>(1);
    }

    /**
     * @brief Square waveform.
     */
    inline T processSquare() const noexcept
    {
        return
            (phase_ < static_cast<T>(0.5))
            ? static_cast<T>(1)
            : static_cast<T>(-1);
    }

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T rateHz_ =
        static_cast<T>(1);

    T depth_ =
        static_cast<T>(1);

    T phase_ =
        static_cast<T>(0);

    T phaseIncrement_ =
        static_cast<T>(0);

    LFOWaveform waveform_ =
        LFOWaveform::Sine;
};

} // namespace cvdsp::modulation

namespace cvdsp
{
template<typename T>
using LFO = modulation::LFO<T>;

using LFOWaveform = modulation::LFOWaveform;
} // namespace cvdsp

#endif
