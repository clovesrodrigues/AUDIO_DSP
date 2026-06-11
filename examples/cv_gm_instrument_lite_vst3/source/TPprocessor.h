//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "SoundFontFolderScanner.h"
#include "SoundFontEngine.h"
#include "CV_DSP/Filters/Biquad.hpp"
#include "CV_DSP/Reverb/HallReverb.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace CV {

class CVGMInstrumentLiteProcessor : public Steinberg::Vst::AudioEffect
{
public:
    CVGMInstrumentLiteProcessor ();
    ~CVGMInstrumentLiteProcessor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new CVGMInstrumentLiteProcessor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
    static constexpr std::size_t kMaxMidiEventsPerBlock = 512;
    static constexpr std::size_t kRenderChunkFrames = 256;

    enum class MidiEventKind
    {
        NoteOff,
        NoteOn,
        ControlChange,
        PitchBend,
        AllNotesOff
    };

    struct MidiPerformanceEvent
    {
        std::size_t sampleOffset {0};
        MidiEventKind kind {MidiEventKind::NoteOff};
        std::uint8_t note {0};
        std::uint8_t controller {0};
        std::uint8_t value {0};
        float velocity {0.0f};
        float pitchBend {0.0f};
    };

    void applyParameterChanges (Steinberg::Vst::IParameterChanges* parameterChanges) noexcept;
    void clearOutputs (Steinberg::Vst::ProcessData& data) noexcept;
    void rescanSoundFonts ();
    bool loadSelectedSoundFont ();
    std::size_t collectMidiEvents (Steinberg::Vst::IEventList* inputEvents, Steinberg::int32 numSamples) noexcept;
    void pushMidiEventSorted (const MidiPerformanceEvent& event, std::size_t& count) noexcept;
    void applyMidiEvent (const MidiPerformanceEvent& event) noexcept;
    void handleControlChange (std::uint8_t controller, std::uint8_t value) noexcept;
    void releaseSustainedNotes () noexcept;
    void resetPerformanceState () noexcept;
    void prepareAudioControls () noexcept;
    void updateAudioControls () noexcept;
    bool processAudioChunk (std::size_t frameCount) noexcept;
    bool renderSegment (Steinberg::Vst::ProcessData& data, std::size_t startSample, std::size_t frameCount) noexcept;
    void updateSilenceFlags (Steinberg::Vst::ProcessData& data, bool blockHasSignal) noexcept;
    std::size_t findSoundFontIndexByFileName (const std::string& fileName) const noexcept;
    std::string selectedSoundFontFileName () const;
    void writeSelectionCache () const;

    double sampleRate_ {44100.0};
    float volume_ {1.0f};
    float bassGainDb_ {0.0f};
    float midGainDb_ {0.0f};
    float trebleGainDb_ {0.0f};
    float room_ {0.0f};
    float rescanToggle_ {0.0f};
    float midiVolume_ {1.0f};
    float midiExpression_ {1.0f};
    float pitchBend_ {0.0f};
    bool sustainPedalDown_ {false};
    std::filesystem::path soundFontsFolder_ {};
    std::filesystem::path soundFontsDataFolder_ {};
    SoundFontEngine soundFontEngine_ {};
    std::array<MidiPerformanceEvent, kMaxMidiEventsPerBlock> midiEvents_ {};
    std::array<bool, 128> activeNotes_ {};
    std::array<bool, 128> sustainedNotes_ {};
    std::array<float, kRenderChunkFrames * 2> renderScratch_ {};
    std::array<float, kRenderChunkFrames> renderLeft_ {};
    std::array<float, kRenderChunkFrames> renderRight_ {};
    std::array<cvdsp::filters::Biquad<float>, 2> bassEq_ {};
    std::array<cvdsp::filters::Biquad<float>, 2> midEq_ {};
    std::array<cvdsp::filters::Biquad<float>, 2> trebleEq_ {};
    cvdsp::reverb::HallReverb<float> hallReverb_ {};
    std::size_t midiEventCount_ {0};
    std::vector<SoundFontFileInfo> soundFontFiles_ {};
    std::size_t selectedSoundFontIndex_ {0};
    std::size_t selectedPresetListIndex_ {0};
};

} // namespace CV
