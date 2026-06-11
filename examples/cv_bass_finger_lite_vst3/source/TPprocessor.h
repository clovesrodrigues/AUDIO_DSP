//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Manager/MidiEvent.hpp"
#include "CV_DSP/Manager/MonoNoteTracker.hpp"
#include "CV_DSP/Synthesis/Bass/BassFingerVoice.hpp"
#include "CV_DSP/Reverb/RoomReverb.hpp"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

#include <array>
#include <cstddef>

namespace CV {

class CVBassFingerLiteProcessor : public Steinberg::Vst::AudioEffect
{
public:
    CVBassFingerLiteProcessor ();
    ~CVBassFingerLiteProcessor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new CVBassFingerLiteProcessor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

protected:
    static constexpr std::size_t kMaxMidiEventsPerBlock = 256;

    using MidiEvent = cvdsp::manager::MidiEvent;
    using MonoTracker = cvdsp::manager::MonoNoteTracker<16>;
    using BassVoice = cvdsp::synthesis::bass::BassFingerVoice<float>;
    using RoomReverb = cvdsp::reverb::RoomReverb<float>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 12, 128>;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;
    std::size_t collectMidiEvents (Steinberg::Vst::IEventList* inputEvents,
                                   Steinberg::int32 numSamples) noexcept;
    void pushMidiEventSorted (const MidiEvent& event, std::size_t& count) noexcept;
    void applyMidiEventToVoice (const MidiEvent& event) noexcept;
    float renderSample () noexcept;
    bool processRoomOnMainOutput (Steinberg::Vst::ProcessData& data, std::size_t sampleCount) noexcept;

    std::array<MidiEvent, kMaxMidiEventsPerBlock> midiEvents_ {};
    std::size_t midiEventCount_ {0};
    MonoTracker noteTracker_ {};
    BassVoice bassVoice_ {};
    RoomReverb roomReverb_ {};
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};
    float tone_ {0.38f};
    float attackMs_ {3.0f};
    float velocitySensitivity_ {0.75f};
    float outputGainDb_ {-6.0f};
    float compression_ {0.15f};
    float drive_ {0.0f};
    float bassGainDb_ {0.0f};
    float midGainDb_ {0.0f};
    float trebleGainDb_ {0.0f};
    float fingerNoise_ {0.0f};
    float humanize_ {0.04f};
    float roomMix_ {0.0f};
};

} // namespace CV
