#ifndef CVDSP_MANAGER_MIDIEVENT_HPP
#define CVDSP_MANAGER_MIDIEVENT_HPP

/**
 * @file MidiEvent.hpp
 * @brief Neutral, sample-accurate MIDI event representation for CV_DSP instruments.
 *
 * Header-only
 * C++20
 * Real-time safe
 * No dynamic allocation
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../Core/Types.hpp"

namespace cvdsp::manager
{

/**
 * @brief Supported neutral MIDI event categories.
 */
enum class MidiEventType : std::uint8_t
{
    NoteOff,
    NoteOn,
    PitchBend,
    ControlChange
};

/**
 * @brief Framework-neutral musical event for sample-accurate dispatch.
 *
 * The structure intentionally contains only primitive values so VST3, CLAP,
 * standalone tools, and tests can share the same instrument code without
 * including host SDK headers in CV_DSP.
 */
struct MidiEvent
{
    std::size_t sampleOffset = 0;      ///< Block-relative sample position.
    MidiEventType type = MidiEventType::NoteOff;
    std::uint8_t channel = 0;          ///< MIDI/event channel in the [0, 15] range when available.
    std::uint8_t note = 0;             ///< MIDI note number in the [0, 127] range.
    f32 value = 0.0f;                  ///< Velocity/CC normalized value or pitch bend in [-1, 1].
    std::int32_t noteId = -1;          ///< Host note identifier when available.
};

/**
 * @brief Clamp and normalize one MIDI event for safe real-time consumption.
 */
[[nodiscard]] inline MidiEvent sanitizeMidiEvent(
    MidiEvent event,
    const std::size_t blockSize = 0) noexcept
{
    if (blockSize > 0 && event.sampleOffset >= blockSize)
        event.sampleOffset = blockSize - 1;

    event.channel = static_cast<std::uint8_t>(std::min<std::uint8_t>(event.channel, 15));
    event.note = static_cast<std::uint8_t>(std::min<std::uint8_t>(event.note, 127));

    if (event.type == MidiEventType::PitchBend)
        event.value = std::clamp(event.value, -1.0f, 1.0f);
    else
        event.value = std::clamp(event.value, 0.0f, 1.0f);

    return event;
}

} // namespace cvdsp::manager

#endif // CVDSP_MANAGER_MIDIEVENT_HPP
