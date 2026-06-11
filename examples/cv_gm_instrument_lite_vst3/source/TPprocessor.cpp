//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#endif

using namespace Steinberg;

namespace CV {
namespace {

constexpr int32 kStateVersion = 2;
constexpr int32 kMaxStateStringBytes = 1024;

bool writeStateString (IBStreamer& streamer, const std::string& value)
{
    const auto bytesToWrite = static_cast<int32> (std::min<std::size_t> (value.size(), kMaxStateStringBytes));
    if (!streamer.writeInt32 (bytesToWrite))
        return false;

    if (bytesToWrite == 0)
        return true;

    return streamer.writeRaw (value.data(), bytesToWrite) == bytesToWrite;
}

bool readStateString (IBStreamer& streamer, std::string& value)
{
    int32 bytesToRead = 0;
    if (!streamer.readInt32 (bytesToRead))
        return false;

    bytesToRead = std::clamp<int32> (bytesToRead, 0, kMaxStateStringBytes);
    value.assign (static_cast<std::size_t> (bytesToRead), '\0');
    if (bytesToRead == 0)
        return true;

    return streamer.readRaw (value.data(), bytesToRead) == bytesToRead;
}

float normalizedToEqGain (Vst::ParamValue normalized) noexcept
{
    return static_cast<float> ((std::clamp (normalized, 0.0, 1.0) * 24.0) - 12.0);
}

Vst::ParamValue eqGainToNormalized (float gainDb) noexcept
{
    return (std::clamp (static_cast<double> (gainDb), -12.0, 12.0) + 12.0) / 24.0;
}

std::uint8_t clampToMidiByte (const int32 value) noexcept
{
    return static_cast<std::uint8_t> (std::clamp<int32> (value, 0, 127));
}

std::uint8_t clampToControllerByte (const int32 value) noexcept
{
    return static_cast<std::uint8_t> (std::clamp<int32> (value, 0, 127));
}

float normalizeControllerValue (const std::uint8_t value) noexcept
{
    return static_cast<float> (value) / 127.0f;
}

float normalizePitchBend (const int32 lsb, const int32 msb) noexcept
{
    const int32 bend14 = std::clamp<int32> (lsb, 0, 127) | (std::clamp<int32> (msb, 0, 127) << 7);
    return std::clamp ((static_cast<float> (bend14) - 8192.0f) / 8192.0f, -1.0f, 1.0f);
}

float normalizeExpressionBend (const std::uint8_t value) noexcept
{
    constexpr int center = 64;
    constexpr int deadZone = 2;
    const int centered = static_cast<int> (value) - center;
    if (std::abs (centered) <= deadZone)
        return 0.0f;

    const float denominator = centered > 0 ? 63.0f : 64.0f;
    return std::clamp (static_cast<float> (centered) / denominator, -1.0f, 1.0f);
}

std::size_t normalizedToListIndex (Vst::ParamValue normalized, std::size_t itemCount) noexcept
{
    if (itemCount == 0)
        return 0;

    const auto maxIndex = static_cast<double> (itemCount - 1);
    const auto index = static_cast<std::size_t> (std::lround (std::clamp (normalized, 0.0, 1.0) * maxIndex));
    return std::min (index, itemCount - 1);
}

float listIndexToNormalized (std::size_t index, std::size_t itemCount) noexcept
{
    if (itemCount <= 1)
        return 0.0f;

    return static_cast<float> (std::min (index, itemCount - 1)) / static_cast<float> (itemCount - 1);
}

std::size_t clampSampleOffset (const int32 sampleOffset, const int32 numSamples) noexcept
{
    if (numSamples <= 0)
        return 0;

    return static_cast<std::size_t> (std::clamp<int32> (sampleOffset, 0, numSamples - 1));
}

std::filesystem::path resolvePluginModulePath ()
{
#if defined(_WIN32)
    HMODULE moduleHandle = nullptr;
    const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA (flags, reinterpret_cast<LPCSTR> (&resolvePluginModulePath), &moduleHandle) && moduleHandle)
    {
        char modulePath[MAX_PATH] {};
        const DWORD length = GetModuleFileNameA (moduleHandle, modulePath, MAX_PATH);
        if (length > 0)
            return std::filesystem::path (modulePath);
    }
#elif defined(__APPLE__) || defined(__linux__)
    Dl_info info {};
    if (dladdr (reinterpret_cast<void*> (&resolvePluginModulePath), &info) && info.dli_fname)
        return std::filesystem::path (info.dli_fname);
#endif

    return std::filesystem::current_path() / "CV_GM_Instrument_Lite.vst3";
}
} // namespace

CVGMInstrumentLiteProcessor::CVGMInstrumentLiteProcessor ()
{
    setControllerClass (kCVGMInstrumentLiteControllerUID);
    soundFontsFolder_ = SoundFontFolderScanner::resolveSiblingSoundFontsFolder (resolvePluginModulePath ());
    soundFontsDataFolder_ = SoundFontFolderScanner::resolveDataFolder (soundFontsFolder_);
    rescanSoundFonts ();
}

CVGMInstrumentLiteProcessor::~CVGMInstrumentLiteProcessor () {}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    addEventInput (STR16 ("MIDI In"), 1);

    return result;
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::setActive (TBool state)
{
    if (state)
    {
        soundFontEngine_.reset ();
        resetPerformanceState ();
        midiEventCount_ = 0;
        (void)loadSelectedSoundFont ();
    }

    return AudioEffect::setActive (state);
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    soundFontEngine_.prepare (sampleRate_);
    prepareAudioControls ();
    (void)loadSelectedSoundFont ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::process (Vst::ProcessData& data)
{
    if (data.symbolicSampleSize != Vst::kSample32)
        return kResultFalse;

    applyParameterChanges (data.inputParameterChanges);
    clearOutputs (data);

    if (!soundFontEngine_.isLoaded())
    {
        updateSilenceFlags (data, false);
        return kResultOk;
    }

    const auto sampleCount = static_cast<std::size_t> (std::max<int32> (data.numSamples, 0));
    midiEventCount_ = collectMidiEvents (data.inputEvents, data.numSamples);

    std::size_t cursor = 0;
    std::size_t eventIndex = 0;
    bool blockHasSignal = false;

    while (eventIndex < midiEventCount_)
    {
        const auto eventSample = std::min (midiEvents_[eventIndex].sampleOffset, sampleCount);
        blockHasSignal = renderSegment (data, cursor, eventSample - cursor) || blockHasSignal;

        while (eventIndex < midiEventCount_ && midiEvents_[eventIndex].sampleOffset == eventSample)
        {
            applyMidiEvent (midiEvents_[eventIndex]);
            ++eventIndex;
        }

        cursor = eventSample;
    }

    blockHasSignal = renderSegment (data, cursor, sampleCount - cursor) || blockHasSignal;
    updateSilenceFlags (data, blockHasSignal);
    return kResultOk;
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float volume = 1.0f;
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;
    float room = 0.0f;
    float soundFontIndex = 0.0f;
    float instrumentIndex = 0.0f;

    (void)streamer.readFloat (volume);
    (void)streamer.readFloat (bass);
    (void)streamer.readFloat (mid);
    (void)streamer.readFloat (treble);
    (void)streamer.readFloat (room);
    (void)streamer.readFloat (soundFontIndex);
    (void)streamer.readFloat (instrumentIndex);

    int32 stateVersion = 1;
    std::string savedSoundFontFileName;
    std::string savedPresetLabel;
    const auto extendedStatePosition = streamer.tell ();
    if (streamer.readInt32 (stateVersion) && stateVersion >= 2)
    {
        (void)readStateString (streamer, savedSoundFontFileName);
        (void)readStateString (streamer, savedPresetLabel);
    }
    else
    {
        streamer.seek (extendedStatePosition, kSeekSet);
    }

    volume_ = std::clamp (volume, 0.0f, 1.0f);
    bassGainDb_ = std::clamp (bass, -12.0f, 12.0f);
    midGainDb_ = std::clamp (mid, -12.0f, 12.0f);
    trebleGainDb_ = std::clamp (treble, -12.0f, 12.0f);
    room_ = std::clamp (room, 0.0f, 1.0f);
    selectedSoundFontIndex_ = savedSoundFontFileName.empty()
        ? normalizedToListIndex (soundFontIndex, soundFontFiles_.size ())
        : findSoundFontIndexByFileName (savedSoundFontFileName);
    (void)loadSelectedSoundFont ();
    selectedPresetListIndex_ = normalizedToListIndex (instrumentIndex, soundFontEngine_.presets().size ());
    (void)soundFontEngine_.selectPresetByListIndex (selectedPresetListIndex_);
    updateAudioControls ();
    return kResultOk;
}

tresult PLUGIN_API CVGMInstrumentLiteProcessor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    writeSelectionCache ();

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (volume_) || !streamer.writeFloat (bassGainDb_) ||
        !streamer.writeFloat (midGainDb_) || !streamer.writeFloat (trebleGainDb_) ||
        !streamer.writeFloat (room_) ||
        !streamer.writeFloat (listIndexToNormalized (selectedSoundFontIndex_, soundFontFiles_.size())) ||
        !streamer.writeFloat (listIndexToNormalized (selectedPresetListIndex_, soundFontEngine_.presets().size())) ||
        !streamer.writeInt32 (kStateVersion) ||
        !writeStateString (streamer, selectedSoundFontFileName ()) ||
        !writeStateString (streamer, (soundFontEngine_.presets().empty() || soundFontEngine_.selectedPresetListIndex() >= soundFontEngine_.presets().size()) ? std::string {} : soundFontEngine_.presets()[soundFontEngine_.selectedPresetListIndex()].displayName))
        return kResultFalse;

    return kResultOk;
}

void CVGMInstrumentLiteProcessor::applyParameterChanges (Vst::IParameterChanges* parameterChanges) noexcept
{
    if (!parameterChanges)
        return;

    const int32 parameterCount = parameterChanges->getParameterCount ();
    for (int32 parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
    {
        Vst::IParamValueQueue* queue = parameterChanges->getParameterData (parameterIndex);
        if (!queue || queue->getPointCount () <= 0)
            continue;

        int32 sampleOffset = 0;
        Vst::ParamValue normalized = 0.0;
        if (queue->getPoint (queue->getPointCount () - 1, sampleOffset, normalized) != kResultTrue)
            continue;

        switch (queue->getParameterId ())
        {
            case kParamGMSoundFont:
                rescanSoundFonts ();
                selectedSoundFontIndex_ = normalizedToListIndex (normalized, soundFontFiles_.size ());
                selectedPresetListIndex_ = 0;
                (void)loadSelectedSoundFont ();
                break;
            case kParamGMInstrument:
                selectedPresetListIndex_ = normalizedToListIndex (normalized, soundFontEngine_.presets().size ());
                (void)soundFontEngine_.selectPresetByListIndex (selectedPresetListIndex_);
                break;
            case kParamGMRescan:
                rescanToggle_ = static_cast<float> (std::clamp (normalized, 0.0, 1.0));
                rescanSoundFonts ();
                selectedSoundFontIndex_ = std::min (selectedSoundFontIndex_, soundFontFiles_.empty() ? std::size_t {0} : soundFontFiles_.size() - 1);
                (void)loadSelectedSoundFont ();
                break;
            case kParamGMVolume:
                volume_ = static_cast<float> (std::clamp (normalized, 0.0, 1.0));
                break;
            case kParamGMBass:
                bassGainDb_ = normalizedToEqGain (normalized);
                break;
            case kParamGMMid:
                midGainDb_ = normalizedToEqGain (normalized);
                break;
            case kParamGMTreble:
                trebleGainDb_ = normalizedToEqGain (normalized);
                break;
            case kParamGMRoom:
                room_ = static_cast<float> (std::clamp (normalized, 0.0, 1.0));
                break;
            default:
                break;
        }
    }

    updateAudioControls ();
}

void CVGMInstrumentLiteProcessor::rescanSoundFonts ()
{
    const auto selectedFileName = selectedSoundFontFileName ();
    soundFontFiles_ = SoundFontFolderScanner::scanSoundFonts (soundFontsFolder_);
    selectedSoundFontIndex_ = selectedFileName.empty()
        ? std::min (selectedSoundFontIndex_, soundFontFiles_.empty() ? std::size_t {0} : soundFontFiles_.size() - 1)
        : findSoundFontIndexByFileName (selectedFileName);
}


std::size_t CVGMInstrumentLiteProcessor::findSoundFontIndexByFileName (const std::string& fileName) const noexcept
{
    if (fileName.empty())
        return 0;

    for (std::size_t index = 0; index < soundFontFiles_.size(); ++index)
    {
        if (soundFontFiles_[index].fileName == fileName)
            return index;
    }

    return std::min (selectedSoundFontIndex_, soundFontFiles_.empty() ? std::size_t {0} : soundFontFiles_.size() - 1);
}

std::string CVGMInstrumentLiteProcessor::selectedSoundFontFileName () const
{
    if (soundFontFiles_.empty() || selectedSoundFontIndex_ >= soundFontFiles_.size())
        return {};

    return soundFontFiles_[selectedSoundFontIndex_].fileName;
}

void CVGMInstrumentLiteProcessor::writeSelectionCache () const
{
    std::error_code error;
    std::filesystem::create_directories (soundFontsDataFolder_, error);
    if (error)
        return;

    std::ofstream file (soundFontsDataFolder_ / "last_state.txt", std::ios::trunc);
    if (!file)
        return;

    file << "sound_font=" << selectedSoundFontFileName () << '\n';
    file << "sound_font_index=" << selectedSoundFontIndex_ << '\n';
    file << "preset_list_index=" << selectedPresetListIndex_ << '\n';
    file << "volume=" << volume_ << '\n';
    file << "bass_db=" << bassGainDb_ << '\n';
    file << "mid_db=" << midGainDb_ << '\n';
    file << "treble_db=" << trebleGainDb_ << '\n';
    file << "room=" << room_ << '\n';
}

bool CVGMInstrumentLiteProcessor::loadSelectedSoundFont ()
{
    if (soundFontFiles_.empty())
        return false;

    const auto selectedIndex = std::min (selectedSoundFontIndex_, soundFontFiles_.size() - 1);
    if (soundFontEngine_.isLoaded() && soundFontEngine_.loadedPath() == soundFontFiles_[selectedIndex].path)
        return true;

    return soundFontEngine_.loadSoundFont (soundFontFiles_[selectedIndex].path);
}

std::size_t CVGMInstrumentLiteProcessor::collectMidiEvents (Vst::IEventList* inputEvents, int32 numSamples) noexcept
{
    if (!inputEvents)
        return 0;

    std::size_t count = 0;
    const int32 eventCount = inputEvents->getEventCount ();

    for (int32 index = 0; index < eventCount; ++index)
    {
        Vst::Event event {};
        if (inputEvents->getEvent (index, event) != kResultOk)
            continue;

        MidiPerformanceEvent midiEvent {};
        midiEvent.sampleOffset = clampSampleOffset (event.sampleOffset, numSamples);

        switch (event.type)
        {
            case Vst::Event::kNoteOnEvent:
                midiEvent.note = clampToMidiByte (event.noteOn.pitch);
                midiEvent.velocity = std::clamp (event.noteOn.velocity, 0.0f, 1.0f);
                midiEvent.kind = midiEvent.velocity > 0.0f ? MidiEventKind::NoteOn : MidiEventKind::NoteOff;
                pushMidiEventSorted (midiEvent, count);
                break;

            case Vst::Event::kNoteOffEvent:
                midiEvent.note = clampToMidiByte (event.noteOff.pitch);
                midiEvent.velocity = std::clamp (event.noteOff.velocity, 0.0f, 1.0f);
                midiEvent.kind = MidiEventKind::NoteOff;
                pushMidiEventSorted (midiEvent, count);
                break;

            case Vst::Event::kLegacyMIDICCOutEvent:
                if (event.midiCCOut.controlNumber == Vst::kPitchBend)
                {
                    midiEvent.kind = MidiEventKind::PitchBend;
                    midiEvent.pitchBend = normalizePitchBend (event.midiCCOut.value, event.midiCCOut.value2);
                }
                else if (event.midiCCOut.controlNumber == Vst::kCtrlAllNotesOff ||
                         event.midiCCOut.controlNumber == Vst::kCtrlAllSoundsOff ||
                         event.midiCCOut.controlNumber == Vst::kCtrlOmniModeOff ||
                         event.midiCCOut.controlNumber == Vst::kCtrlOmniModeOn ||
                         event.midiCCOut.controlNumber == Vst::kCtrlPolyModeOnOff ||
                         event.midiCCOut.controlNumber == Vst::kCtrlPolyModeOn)
                {
                    midiEvent.kind = MidiEventKind::AllNotesOff;
                }
                else
                {
                    midiEvent.kind = MidiEventKind::ControlChange;
                    midiEvent.controller = event.midiCCOut.controlNumber;
                    midiEvent.value = clampToControllerByte (event.midiCCOut.value);
                }

                pushMidiEventSorted (midiEvent, count);
                break;

            default:
                break;
        }
    }

    return count;
}

void CVGMInstrumentLiteProcessor::pushMidiEventSorted (const MidiPerformanceEvent& event, std::size_t& count) noexcept
{
    if (count >= kMaxMidiEventsPerBlock)
        return;

    std::size_t insertIndex = count;
    while (insertIndex > 0 && midiEvents_[insertIndex - 1].sampleOffset > event.sampleOffset)
    {
        midiEvents_[insertIndex] = midiEvents_[insertIndex - 1];
        --insertIndex;
    }

    midiEvents_[insertIndex] = event;
    ++count;
}

void CVGMInstrumentLiteProcessor::applyMidiEvent (const MidiPerformanceEvent& event) noexcept
{
    switch (event.kind)
    {
        case MidiEventKind::NoteOn:
            activeNotes_[event.note] = true;
            sustainedNotes_[event.note] = false;
            soundFontEngine_.noteOn (event.note, event.velocity);
            break;

        case MidiEventKind::NoteOff:
            activeNotes_[event.note] = false;
            if (sustainPedalDown_)
            {
                sustainedNotes_[event.note] = true;
                break;
            }

            sustainedNotes_[event.note] = false;
            soundFontEngine_.noteOff (event.note);
            break;

        case MidiEventKind::ControlChange:
            handleControlChange (event.controller, event.value);
            break;

        case MidiEventKind::PitchBend:
            pitchBend_ = event.pitchBend;
            soundFontEngine_.setPitchBend (pitchBend_);
            break;

        case MidiEventKind::AllNotesOff:
            soundFontEngine_.allNotesOff ();
            resetPerformanceState ();
            break;
    }
}

void CVGMInstrumentLiteProcessor::handleControlChange (std::uint8_t controller, std::uint8_t value) noexcept
{
    soundFontEngine_.setMidiController (controller, value);

    switch (controller)
    {
        case Vst::kCtrlSustainOnOff:
        {
            const bool sustainDown = value >= 64;
            if (sustainPedalDown_ && !sustainDown)
                releaseSustainedNotes ();
            sustainPedalDown_ = sustainDown;
            break;
        }

        case Vst::kCtrlVolume:
            midiVolume_ = normalizeControllerValue (value);
            break;

        case Vst::kCtrlExpression:
            pitchBend_ = normalizeExpressionBend (value);
            soundFontEngine_.setPitchBend (pitchBend_);
            break;

        case Vst::kCtrlEff1Depth:
            room_ = normalizeControllerValue (value);
            hallReverb_.setWet (std::clamp (room_, 0.0f, 1.0f) * 0.32f);
            break;

        case Vst::kCtrlResetAllCtrlers:
            midiVolume_ = 1.0f;
            midiExpression_ = 1.0f;
            pitchBend_ = 0.0f;
            sustainPedalDown_ = false;
            releaseSustainedNotes ();
            soundFontEngine_.setPitchBend (pitchBend_);
            break;

        default:
            break;
    }
}

void CVGMInstrumentLiteProcessor::releaseSustainedNotes () noexcept
{
    for (std::size_t note = 0; note < sustainedNotes_.size(); ++note)
    {
        if (sustainedNotes_[note] && !activeNotes_[note])
            soundFontEngine_.noteOff (static_cast<std::uint8_t> (note));

        sustainedNotes_[note] = false;
    }
}

void CVGMInstrumentLiteProcessor::resetPerformanceState () noexcept
{
    activeNotes_.fill (false);
    sustainedNotes_.fill (false);
    sustainPedalDown_ = false;
    midiVolume_ = 1.0f;
    midiExpression_ = 1.0f;
    pitchBend_ = 0.0f;
    soundFontEngine_.setPitchBend (pitchBend_);
}


void CVGMInstrumentLiteProcessor::prepareAudioControls () noexcept
{
    const float sr = static_cast<float> (sampleRate_ > 0.0 ? sampleRate_ : 44100.0);

    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        bassEq_[channel].prepare (sr);
        bassEq_[channel].setType (cvdsp::filters::BiquadType::LowShelf);

        midEq_[channel].prepare (sr);
        midEq_[channel].setType (cvdsp::filters::BiquadType::PeakingEQ);

        trebleEq_[channel].prepare (sr);
        trebleEq_[channel].setType (cvdsp::filters::BiquadType::HighShelf);
    }

    cvdsp::ProcessContext<float> context {};
    context.sampleRate = sr;
    context.numChannels = 2;
    hallReverb_.prepare (context);
    hallReverb_.setHallSize (0.34f);
    hallReverb_.setRT60 (1.15f);
    hallReverb_.setDamping (0.62f);
    hallReverb_.setPreDelay (8.0f);
    hallReverb_.setWidth (0.82f);

    updateAudioControls ();
}

void CVGMInstrumentLiteProcessor::updateAudioControls () noexcept
{
    const float sr = static_cast<float> (sampleRate_ > 0.0 ? sampleRate_ : 44100.0);

    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        bassEq_[channel].setFrequency (115.0f);
        bassEq_[channel].setQ (0.70710678f);
        bassEq_[channel].setGainDB (std::clamp (bassGainDb_, -12.0f, 12.0f));
        bassEq_[channel].updateCoefficients ();

        midEq_[channel].setFrequency (850.0f);
        midEq_[channel].setQ (0.82f);
        midEq_[channel].setGainDB (std::clamp (midGainDb_, -12.0f, 12.0f));
        midEq_[channel].updateCoefficients ();

        trebleEq_[channel].setFrequency (4200.0f);
        trebleEq_[channel].setQ (0.70710678f);
        trebleEq_[channel].setGainDB (std::clamp (trebleGainDb_, -12.0f, 12.0f));
        trebleEq_[channel].updateCoefficients ();
    }

    (void)sr;
    hallReverb_.setDry (1.0f);
    hallReverb_.setWet (std::clamp (room_, 0.0f, 1.0f) * 0.32f);
}

bool CVGMInstrumentLiteProcessor::processAudioChunk (std::size_t frameCount) noexcept
{
    if (frameCount == 0)
        return false;

    bool hasSignal = false;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        float left = renderScratch_[frame * 2] * volume_ * midiVolume_ * midiExpression_;
        float right = renderScratch_[frame * 2 + 1] * volume_ * midiVolume_ * midiExpression_;

        left = bassEq_[0].process (left);
        left = midEq_[0].process (left);
        left = trebleEq_[0].process (left);

        right = bassEq_[1].process (right);
        right = midEq_[1].process (right);
        right = trebleEq_[1].process (right);

        renderLeft_[frame] = left;
        renderRight_[frame] = right;
    }

    std::array<float*, 2> channels { renderLeft_.data(), renderRight_.data() };
    cvdsp::AudioBufferView<float> buffer (channels.data (), 2, frameCount);
    hallReverb_.processBlock (buffer);

    for (std::size_t frame = 0; frame < frameCount; ++frame)
        hasSignal = hasSignal || std::abs (renderLeft_[frame]) > 1.0e-8f || std::abs (renderRight_[frame]) > 1.0e-8f;

    return hasSignal;
}

bool CVGMInstrumentLiteProcessor::renderSegment (Vst::ProcessData& data, std::size_t startSample, std::size_t frameCount) noexcept
{
    if (frameCount == 0 || data.numOutputs <= 0)
        return false;

    bool hasSignal = false;
    std::size_t rendered = 0;

    while (rendered < frameCount)
    {
        const auto framesThisChunk = std::min (kRenderChunkFrames, frameCount - rendered);
        std::fill_n (renderScratch_.data(), framesThisChunk * 2, 0.0f);
        soundFontEngine_.renderInterleavedStereo (renderScratch_.data(), static_cast<int> (framesThisChunk), false);

        hasSignal = processAudioChunk (framesThisChunk) || hasSignal;

        for (int32 bus = 0; bus < data.numOutputs; ++bus)
        {
            auto& output = data.outputs[bus];
            if (!output.channelBuffers32)
                continue;

            for (int32 channel = 0; channel < output.numChannels; ++channel)
            {
                float* destination = output.channelBuffers32[channel];
                if (!destination)
                    continue;

                for (std::size_t frame = 0; frame < framesThisChunk; ++frame)
                {
                    const float sample = channel == 0
                        ? renderLeft_[frame]
                        : (channel == 1 ? renderRight_[frame] : 0.0f);
                    destination[startSample + rendered + frame] = sample;
                }
            }
        }

        rendered += framesThisChunk;
    }

    return hasSignal;
}

void CVGMInstrumentLiteProcessor::updateSilenceFlags (Vst::ProcessData& data, bool blockHasSignal) noexcept
{
    for (int32 bus = 0; bus < data.numOutputs; ++bus)
    {
        data.outputs[bus].silenceFlags = blockHasSignal
            ? 0
            : (data.outputs[bus].numChannels >= 64
                ? ~static_cast<uint64> (0)
                : ((static_cast<uint64> (1) << data.outputs[bus].numChannels) - static_cast<uint64> (1)));
    }
}

void CVGMInstrumentLiteProcessor::clearOutputs (Vst::ProcessData& data) noexcept
{
    for (int32 bus = 0; bus < data.numOutputs; ++bus)
    {
        auto& output = data.outputs[bus];
        if (output.channelBuffers32)
        {
            for (int32 channel = 0; channel < output.numChannels; ++channel)
            {
                if (!output.channelBuffers32[channel])
                    continue;

                std::fill_n (output.channelBuffers32[channel], static_cast<std::size_t> (std::max<int32> (data.numSamples, 0)), 0.0f);
            }
        }

        output.silenceFlags = output.numChannels >= 64
            ? ~static_cast<uint64> (0)
            : ((static_cast<uint64> (1) << output.numChannels) - static_cast<uint64> (1));
    }
}

} // namespace CV
