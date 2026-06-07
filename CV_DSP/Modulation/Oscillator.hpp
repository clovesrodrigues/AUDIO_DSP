#ifndef CVDSP_MODULATION_OSCILLATOR_HPP
#define CVDSP_MODULATION_OSCILLATOR_HPP

/**
 * @file Oscillator.hpp
 * @brief Digital Oscillator
 *
 * Header-only
 * C++20
 * Real-Time Safe
 * No dynamic allocation
 */

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <algorithm>

namespace cvdsp
{

/**
 * @brief Oscillator waveform types.
 */
enum class OscillatorWaveform
{
    Sine,
    Triangle,
    Saw,
    Square
};

/**
 * @brief Digital oscillator using phase accumulator.
 *
 * Supports:
 * - Sine
 * - Triangle
 * - Saw
 * - Square
 *
 * Template:
 *
 * float
 * double
 */
template<typename T>
class Oscillator
{
    static_assert(
        std::is_floating_point_v<T>,
        "Oscillator requires floating point type");

public:

    constexpr Oscillator() noexcept = default;

    /**
     * @brief Initialize oscillator.
     *
     * @param sampleRate Processing sample rate.
     * @param frequency Initial frequency.
     * @param waveform Waveform type.
     */
    void prepare(
        T sampleRate,
        T frequency,
        OscillatorWaveform waveform) noexcept
    {
        sampleRate_ =
            std::max(
                sampleRate,
                static_cast<T>(1));

        waveform_ = waveform;

        setFrequency(frequency);

        reset();
    }

    /**
     * @brief Reset oscillator state.
     */
    void reset() noexcept
    {
        phase_ = static_cast<T>(0);
    }

    /**
     * @brief Set oscillator frequency.
     *
     * @param frequency Frequency in Hz.
     */
    void setFrequency(
        T frequency) noexcept
    {
        constexpr T kMinFreq =
            static_cast<T>(0);

        const T nyquist =
            sampleRate_ *
            static_cast<T>(0.5);

        frequency_ =
            std::clamp(
                frequency,
                kMinFreq,
                nyquist);

        phaseIncrement_ =
            frequency_ / sampleRate_;
    }

    /**
     * @brief Set waveform.
     *
     * @param waveform New waveform.
     */
    void setWaveform(
        OscillatorWaveform waveform) noexcept
    {
        waveform_ = waveform;
    }

    /**
     * @brief Get current frequency.
     */
    [[nodiscard]]
    T getFrequency() const noexcept
    {
        return frequency_;
    }

    /**
     * @brief Generate one sample.
     */
    inline T process() noexcept
    {
        T output {};

        switch (waveform_)
        {
            case OscillatorWaveform::Sine:
            {
                output = processSine();
                break;
            }

            case OscillatorWaveform::Triangle:
            {
                output = processTriangle();
                break;
            }

            case OscillatorWaveform::Saw:
            {
                output = processSaw();
                break;
            }

            case OscillatorWaveform::Square:
            {
                output = processSquare();
                break;
            }

            default:
            {
                output = static_cast<T>(0);
                break;
            }
        }

        advancePhase();

        return output;
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
     * @brief Sine oscillator.
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
     * @brief Saw oscillator.
     *
     * Range:
     *
     * -1 ... +1
     */
    inline T processSaw() const noexcept
    {
        return
            static_cast<T>(2) * phase_
            -
            static_cast<T>(1);
    }

    /**
     * @brief Square oscillator.
     *
     * 50% duty cycle.
     */
    inline T processSquare() const noexcept
    {
        return
            (phase_ < static_cast<T>(0.5))
                ? static_cast<T>(1)
                : static_cast<T>(-1);
    }

    /**
     * @brief Triangle oscillator.
     *
     * Range:
     *
     * -1 ... +1
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

private:

    T sampleRate_ =
        static_cast<T>(44100);

    T frequency_ =
        static_cast<T>(440);

    T phase_ =
        static_cast<T>(0);

    T phaseIncrement_ =
        static_cast<T>(0);

    OscillatorWaveform waveform_ =
        OscillatorWaveform::Sine;
};

} // namespace cvdsp

#endif
