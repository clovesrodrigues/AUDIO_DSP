#ifndef CVDSP_MANAGER_MONONOTETRACKER_HPP
#define CVDSP_MANAGER_MONONOTETRACKER_HPP

/**
 * @file MonoNoteTracker.hpp
 * @brief Fixed-capacity monophonic note tracker for bass/synth instruments.
 *
 * Header-only
 * C++20
 * Real-time safe
 * No dynamic allocation
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "MidiEvent.hpp"

namespace cvdsp::manager
{

/**
 * @brief Result of applying one neutral MIDI event to the monophonic tracker.
 */
struct MonoNoteTrackerResult
{
    bool noteStarted = false;      ///< A note became active from idle state.
    bool noteChanged = false;      ///< Active pitch changed while notes were held.
    bool noteReleased = false;     ///< Active note stack became empty.
    bool isLegato = false;         ///< New active note arrived while another note was already active.
    std::uint8_t note = 0;         ///< Current active note or released note.
    f32 velocity = 0.0f;           ///< Current active velocity or release velocity.
};

/**
 * @brief Last-note-priority monophonic note stack with fixed storage.
 *
 * @tparam MaxHeldNotes Maximum simultaneous held note records. This is not audio
 * polyphony; it is only the keyboard state used to return to a previous held note
 * after releasing the current one.
 */
template<std::size_t MaxHeldNotes = 16>
class MonoNoteTracker
{
    static_assert(MaxHeldNotes > 0, "MonoNoteTracker requires MaxHeldNotes > 0");

public:
    constexpr MonoNoteTracker() noexcept = default;

    void reset() noexcept
    {
        heldCount_ = 0;
        active_ = false;
        activeNote_ = 0;
        activeVelocity_ = 0.0f;
        activeNoteId_ = -1;
        pitchBend_ = 0.0f;
        for (auto& note : heldNotes_)
            note = HeldNote{};
    }

    [[nodiscard]] MonoNoteTrackerResult handleMidiEvent(const MidiEvent& rawEvent) noexcept
    {
        const MidiEvent event = sanitizeMidiEvent(rawEvent);

        switch (event.type)
        {
            case MidiEventType::NoteOn:
                if (event.value <= 0.0f)
                    return noteOff(event.note, event.value, event.noteId);
                return noteOn(event.note, event.value, event.noteId);

            case MidiEventType::NoteOff:
                return noteOff(event.note, event.value, event.noteId);

            case MidiEventType::PitchBend:
                pitchBend_ = std::clamp(event.value, -1.0f, 1.0f);
                break;

            default:
                break;
        }

        return {};
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] std::uint8_t getActiveNote() const noexcept { return activeNote_; }
    [[nodiscard]] f32 getActiveVelocity() const noexcept { return activeVelocity_; }
    [[nodiscard]] std::int32_t getActiveNoteId() const noexcept { return activeNoteId_; }
    [[nodiscard]] f32 getPitchBend() const noexcept { return pitchBend_; }
    [[nodiscard]] std::size_t getHeldNoteCount() const noexcept { return heldCount_; }

private:
    struct HeldNote
    {
        std::uint8_t note = 0;
        f32 velocity = 0.0f;
        std::int32_t noteId = -1;
    };

    [[nodiscard]] std::size_t findHeldNote(
        const std::uint8_t note,
        const std::int32_t noteId) const noexcept
    {
        for (std::size_t i = 0; i < heldCount_; ++i)
        {
            const bool idMatches = noteId >= 0 && heldNotes_[i].noteId == noteId;
            const bool noteMatches = noteId < 0 && heldNotes_[i].note == note;
            if (idMatches || noteMatches)
                return i;
        }
        return InvalidIndex;
    }

    MonoNoteTrackerResult noteOn(
        const std::uint8_t note,
        const f32 velocity,
        const std::int32_t noteId) noexcept
    {
        const bool wasActive = active_;
        const std::uint8_t previousNote = activeNote_;

        const std::size_t existing = findHeldNote(note, noteId);
        if (existing != InvalidIndex)
            removeHeldNoteAt(existing);

        pushHeldNote({note, std::clamp(velocity, 0.0f, 1.0f), noteId});
        applyTopHeldNote();

        MonoNoteTrackerResult result{};
        result.noteStarted = !wasActive;
        result.noteChanged = wasActive && previousNote != activeNote_;
        result.isLegato = wasActive;
        result.note = activeNote_;
        result.velocity = activeVelocity_;
        return result;
    }

    MonoNoteTrackerResult noteOff(
        const std::uint8_t note,
        const f32 velocity,
        const std::int32_t noteId) noexcept
    {
        const bool wasActive = active_;
        const std::uint8_t previousNote = activeNote_;
        const f32 releaseVelocity = std::clamp(velocity, 0.0f, 1.0f);

        const std::size_t existing = findHeldNote(note, noteId);
        if (existing != InvalidIndex)
            removeHeldNoteAt(existing);

        if (heldCount_ > 0)
            applyTopHeldNote();
        else
        {
            active_ = false;
            activeNoteId_ = -1;
            activeVelocity_ = 0.0f;
        }

        MonoNoteTrackerResult result{};
        result.noteReleased = wasActive && !active_;
        result.noteChanged = wasActive && active_ && previousNote != activeNote_;
        result.isLegato = result.noteChanged;
        result.note = active_ ? activeNote_ : previousNote;
        result.velocity = active_ ? activeVelocity_ : releaseVelocity;
        return result;
    }

    void pushHeldNote(const HeldNote& note) noexcept
    {
        if (heldCount_ >= MaxHeldNotes)
        {
            for (std::size_t i = 1; i < heldCount_; ++i)
                heldNotes_[i - 1] = heldNotes_[i];
            --heldCount_;
        }

        heldNotes_[heldCount_] = note;
        ++heldCount_;
    }

    void removeHeldNoteAt(const std::size_t index) noexcept
    {
        if (index >= heldCount_)
            return;

        for (std::size_t i = index + 1; i < heldCount_; ++i)
            heldNotes_[i - 1] = heldNotes_[i];

        --heldCount_;
        if (heldCount_ < MaxHeldNotes)
            heldNotes_[heldCount_] = HeldNote{};
    }

    void applyTopHeldNote() noexcept
    {
        if (heldCount_ == 0)
            return;

        const HeldNote& top = heldNotes_[heldCount_ - 1];
        active_ = true;
        activeNote_ = top.note;
        activeVelocity_ = top.velocity;
        activeNoteId_ = top.noteId;
    }

    static constexpr std::size_t InvalidIndex = static_cast<std::size_t>(-1);

    std::array<HeldNote, MaxHeldNotes> heldNotes_{};
    std::size_t heldCount_ = 0;
    bool active_ = false;
    std::uint8_t activeNote_ = 0;
    f32 activeVelocity_ = 0.0f;
    std::int32_t activeNoteId_ = -1;
    f32 pitchBend_ = 0.0f;
};

} // namespace cvdsp::manager

#endif // CVDSP_MANAGER_MONONOTETRACKER_HPP
