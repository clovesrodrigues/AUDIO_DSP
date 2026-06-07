#ifndef CVDSP_CORE_PROCESSCONTEXT_HPP
#define CVDSP_CORE_PROCESSCONTEXT_HPP

/**
 * @file ProcessContext.hpp
 * @brief Unified DSP Processing Context
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Provides a common processing context
 * for all CV_DSP modules.
 *
 * Compatible with:
 * - VST3
 * - iPlug2
 * - JUCE
 * - CLAP
 * - Standalone
 */

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace cvdsp
{

/**
 * @brief Processing context shared by all DSP modules.
 *
 * Contains transport, timing and
 * audio engine information.
 *
 * This structure is intentionally
 * POD-like for maximum efficiency.
 */
template<typename T>
struct ProcessContext
{
    static_assert(
        std::is_floating_point_v<T>,
        "ProcessContext requires floating point type");

    /**
     * Audio sample rate.
     *
     * Example:
     *
     * 44100
     * 48000
     * 96000
     */
    T sampleRate =
        static_cast<T>(44100);

    /**
     * Current processing block size.
     */
    std::size_t blockSize =
        0;

    /**
     * Number of active channels.
     */
    std::size_t numChannels =
        0;

    /**
     * Host tempo in BPM.
     */
    T tempo =
        static_cast<T>(120);

    /**
     * Time signature numerator.
     *
     * Example:
     *
     * 4/4 -> 4
     * 3/4 -> 3
     * 7/8 -> 7
     */
    std::uint32_t timeSignatureNumerator =
        4;

    /**
     * Time signature denominator.
     *
     * Example:
     *
     * 4/4 -> 4
     * 7/8 -> 8
     */
    std::uint32_t timeSignatureDenominator =
        4;

    /**
     * Host transport state.
     */
    bool isPlaying =
        false;

    /**
     * Host recording state.
     */
    bool isRecording =
        false;

    /**
     * Sample position.
     *
     * Optional host information.
     */
    std::uint64_t samplePosition =
        0;

    /**
     * PPQ position.
     *
     * Optional host information.
     */
    T ppqPosition =
        static_cast<T>(0);

    /**
     * Bar start PPQ.
     *
     * Optional host information.
     */
    T barStartPPQ =
        static_cast<T>(0);

    /**
     * Seconds since transport start.
     */
    T timeInSeconds =
        static_cast<T>(0);

    /**
     * Reset to defaults.
     */
    constexpr void reset() noexcept
    {
        sampleRate =
            static_cast<T>(44100);

        blockSize =
            0;

        numChannels =
            0;

        tempo =
            static_cast<T>(120);

        timeSignatureNumerator =
            4;

        timeSignatureDenominator =
            4;

        isPlaying =
            false;

        isRecording =
            false;

        samplePosition =
            0;

        ppqPosition =
            static_cast<T>(0);

        barStartPPQ =
            static_cast<T>(0);

        timeInSeconds =
            static_cast<T>(0);
    }

    /**
     * Returns samples per beat.
     */
    [[nodiscard]]
    constexpr T samplesPerBeat() const noexcept
    {
        return
            (
                static_cast<T>(60)
                *
                sampleRate
            )
            /
            tempo;
    }

    /**
     * Returns beats per second.
     */
    [[nodiscard]]
    constexpr T beatsPerSecond() const noexcept
    {
        return
            tempo
            /
            static_cast<T>(60);
    }

    /**
     * Returns seconds per beat.
     */
    [[nodiscard]]
    constexpr T secondsPerBeat() const noexcept
    {
        return
            static_cast<T>(60)
            /
            tempo;
    }

    /**
     * Returns duration of one bar.
     */
    [[nodiscard]]
    constexpr T secondsPerBar() const noexcept
    {
        return
            secondsPerBeat()
            *
            static_cast<T>(
                timeSignatureNumerator);
    }

    /**
     * Returns samples per bar.
     */
    [[nodiscard]]
    constexpr T samplesPerBar() const noexcept
    {
        return
            samplesPerBeat()
            *
            static_cast<T>(
                timeSignatureNumerator);
    }

    /**
     * Returns duration of current block.
     */
    [[nodiscard]]
    constexpr T blockDurationSeconds() const noexcept
    {
        if (sampleRate <= static_cast<T>(0))
        {
            return static_cast<T>(0);
        }

        return
            static_cast<T>(blockSize)
            /
            sampleRate;
    }

    /**
     * Returns Nyquist frequency.
     */
    [[nodiscard]]
    constexpr T nyquist() const noexcept
    {
        return
            sampleRate
            *
            static_cast<T>(0.5);
    }

    /**
     * Returns true if transport running.
     */
    [[nodiscard]]
    constexpr bool playing() const noexcept
    {
        return isPlaying;
    }

    /**
     * Returns true if recording.
     */
    [[nodiscard]]
    constexpr bool recording() const noexcept
    {
        return isRecording;
    }

    /**
     * Returns true if transport stopped.
     */
    [[nodiscard]]
    constexpr bool stopped() const noexcept
    {
        return !isPlaying;
    }

    /**
     * Returns true if context appears valid.
     */
    [[nodiscard]]
    constexpr bool isValid() const noexcept
    {
        return
            sampleRate >
            static_cast<T>(0)
            &&
            blockSize > 0
            &&
            numChannels > 0
            &&
            tempo >
            static_cast<T>(0)
            &&
            timeSignatureNumerator > 0
            &&
            timeSignatureDenominator > 0;
    }

    /**
     * Returns current beat position.
     */
    [[nodiscard]]
    constexpr T beatPosition() const noexcept
    {
        return ppqPosition;
    }

    /**
     * Returns beat position within bar.
     */
    [[nodiscard]]
    constexpr T beatInBar() const noexcept
    {
        return
            ppqPosition
            -
            barStartPPQ;
    }

    /**
     * Returns current measure length in beats.
     */
    [[nodiscard]]
    constexpr T beatsPerBar() const noexcept
    {
        return static_cast<T>(
            timeSignatureNumerator);
    }
};

using ProcessContextF =
    ProcessContext<float>;

using ProcessContextD =
    ProcessContext<double>;

} // namespace cvdsp

#endif
