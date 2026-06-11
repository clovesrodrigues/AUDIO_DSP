#include "CV_DSP/Manager/MonoNoteTracker.hpp"

#include <cassert>
#include <cstddef>

int main()
{
    cvdsp::manager::MonoNoteTracker<4> tracker;

    auto result = tracker.handleMidiEvent({0, cvdsp::manager::MidiEventType::NoteOn, 0, 40, 0.75f, 10});
    assert(result.noteStarted);
    assert(!result.isLegato);
    assert(tracker.isActive());
    assert(tracker.getActiveNote() == 40);
    assert(tracker.getHeldNoteCount() == 1);

    result = tracker.handleMidiEvent({12, cvdsp::manager::MidiEventType::NoteOn, 0, 43, 0.65f, 11});
    assert(!result.noteStarted);
    assert(result.noteChanged);
    assert(result.isLegato);
    assert(tracker.getActiveNote() == 43);
    assert(tracker.getHeldNoteCount() == 2);

    result = tracker.handleMidiEvent({24, cvdsp::manager::MidiEventType::NoteOff, 0, 43, 0.0f, 11});
    assert(!result.noteReleased);
    assert(result.noteChanged);
    assert(result.isLegato);
    assert(tracker.isActive());
    assert(tracker.getActiveNote() == 40);
    assert(tracker.getHeldNoteCount() == 1);

    result = tracker.handleMidiEvent({36, cvdsp::manager::MidiEventType::NoteOff, 0, 40, 0.0f, 10});
    assert(result.noteReleased);
    assert(!tracker.isActive());
    assert(tracker.getHeldNoteCount() == 0);

    result = tracker.handleMidiEvent({48, cvdsp::manager::MidiEventType::PitchBend, 0, 0, 2.0f, -1});
    assert(tracker.getPitchBend() == 1.0f);

    const auto sanitized = cvdsp::manager::sanitizeMidiEvent(
        {999, cvdsp::manager::MidiEventType::NoteOn, 99, 200, 2.0f, -1},
        128);
    assert(sanitized.sampleOffset == 127);
    assert(sanitized.channel == 15);
    assert(sanitized.note == 127);
    assert(sanitized.value == 1.0f);

    return 0;
}
