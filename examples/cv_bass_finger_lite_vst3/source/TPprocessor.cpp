//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;

namespace CV {
namespace {
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;

std::uint8_t clampToMidiByte (const int32 value) noexcept
{
    return static_cast<std::uint8_t> (std::clamp<int32> (value, 0, 127));
}

std::uint8_t clampToMidiChannel (const int32 value) noexcept
{
    return static_cast<std::uint8_t> (std::clamp<int32> (value, 0, 15));
}

std::size_t clampSampleOffset (const int32 sampleOffset, const int32 numSamples) noexcept
{
    if (numSamples <= 0)
        return 0;

    return static_cast<std::size_t> (std::clamp<int32> (sampleOffset, 0, numSamples - 1));
}

} // namespace

CVBassFingerLiteProcessor::CVBassFingerLiteProcessor ()
{
    setControllerClass (kCVBassFingerLiteControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

CVBassFingerLiteProcessor::~CVBassFingerLiteProcessor () {}

tresult PLUGIN_API CVBassFingerLiteProcessor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    addEventInput (STR16 ("MIDI In"), 1);

    return result;
}

tresult PLUGIN_API CVBassFingerLiteProcessor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API CVBassFingerLiteProcessor::setActive (TBool state)
{
    if (state)
    {
        noteTracker_.reset ();
        bassVoice_.reset ();
        roomReverb_.reset ();
        midiEventCount_ = 0;
    }

    return AudioEffect::setActive (state);
}

tresult PLUGIN_API CVBassFingerLiteProcessor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    noteTracker_.reset ();
    bassVoice_.prepare (static_cast<float> (sampleRate_));
    cvdsp::ProcessContext<float> roomContext {};
    roomContext.sampleRate = static_cast<float> (sampleRate_);
    roomContext.numChannels = 2;
    roomReverb_.prepare (roomContext);
    roomReverb_.setRoomSize (0.28f);
    roomReverb_.setDecayTime (0.42f);
    roomReverb_.setDamping (0.68f);
    roomReverb_.setDry (1.0f);
    roomReverb_.setWet (roomMix_ * 0.22f);
    midiEventCount_ = 0;
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API CVBassFingerLiteProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API CVBassFingerLiteProcessor::process (Vst::ProcessData& data)
{
    if (data.symbolicSampleSize != Vst::kSample32)
        return kResultFalse;

    const auto sampleCount = static_cast<std::size_t> (std::max<int32> (data.numSamples, 0));
    parameters_.beginBlock (sampleCount);
    cvdsp::adapters::vst3::VST3ParameterAdapter::adaptParameterChanges (data.inputParameterChanges, parameters_);
    parameters_.processBlockParameters (sampleCount);
    applyParametersToDSP ();

    midiEventCount_ = collectMidiEvents (data.inputEvents, data.numSamples);

    std::size_t eventIndex = 0;
    bool blockHasSignal = bassVoice_.isActive ();

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        while (eventIndex < midiEventCount_ && midiEvents_[eventIndex].sampleOffset <= sample)
        {
            applyMidiEventToVoice (midiEvents_[eventIndex]);
            ++eventIndex;
        }

        const float outputSample = renderSample ();
        blockHasSignal = blockHasSignal || outputSample != 0.0f;

        for (int32 bus = 0; bus < data.numOutputs; ++bus)
        {
            for (int32 channel = 0; channel < data.outputs[bus].numChannels; ++channel)
                data.outputs[bus].channelBuffers32[channel][sample] = outputSample;
        }
    }

    blockHasSignal = processRoomOnMainOutput (data, sampleCount) || blockHasSignal;

    for (int32 bus = 0; bus < data.numOutputs; ++bus)
    {
        data.outputs[bus].silenceFlags = blockHasSignal
            ? 0
            : (data.outputs[bus].numChannels >= 64
                ? ~static_cast<uint64> (0)
                : ((static_cast<uint64> (1) << data.outputs[bus].numChannels) - static_cast<uint64> (1)));
    }

    return kResultOk;
}

tresult PLUGIN_API CVBassFingerLiteProcessor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float tone = 0.38f;
    float attack = 3.0f;
    float velocitySensitivity = 0.75f;
    float outputGain = -6.0f;
    float compression = 0.15f;
    float drive = 0.0f;
    float bassGain = 0.0f;
    float midGain = 0.0f;
    float trebleGain = 0.0f;
    float fingerNoise = 0.0f;
    float humanize = 0.04f;
    float roomMix = 0.0f;
    if (!streamer.readFloat (tone) || !streamer.readFloat (attack) ||
        !streamer.readFloat (velocitySensitivity) || !streamer.readFloat (outputGain))
        return kResultFalse;

    (void)streamer.readFloat (compression);
    (void)streamer.readFloat (drive);
    (void)streamer.readFloat (bassGain);
    (void)streamer.readFloat (midGain);
    (void)streamer.readFloat (trebleGain);
    (void)streamer.readFloat (fingerNoise);
    (void)streamer.readFloat (humanize);
    (void)streamer.readFloat (roomMix);

    (void)parameters_.setImmediateReal (kParamBassTone, tone);
    (void)parameters_.setImmediateReal (kParamBassAttack, attack);
    (void)parameters_.setImmediateReal (kParamBassVelocitySensitivity, velocitySensitivity);
    (void)parameters_.setImmediateReal (kParamBassOutputGain, outputGain);
    (void)parameters_.setImmediateReal (kParamBassCompression, compression);
    (void)parameters_.setImmediateReal (kParamBassDrive, drive);
    (void)parameters_.setImmediateReal (kParamBassLowEQ, bassGain);
    (void)parameters_.setImmediateReal (kParamBassMidEQ, midGain);
    (void)parameters_.setImmediateReal (kParamBassHighEQ, trebleGain);
    (void)parameters_.setImmediateReal (kParamBassFingerNoise, fingerNoise);
    (void)parameters_.setImmediateReal (kParamBassHumanize, humanize);
    (void)parameters_.setImmediateReal (kParamBassRoom, roomMix);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API CVBassFingerLiteProcessor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (tone_) || !streamer.writeFloat (attackMs_) ||
        !streamer.writeFloat (velocitySensitivity_) || !streamer.writeFloat (outputGainDb_) ||
        !streamer.writeFloat (compression_) || !streamer.writeFloat (drive_) ||
        !streamer.writeFloat (bassGainDb_) || !streamer.writeFloat (midGainDb_) ||
        !streamer.writeFloat (trebleGainDb_) || !streamer.writeFloat (fingerNoise_) ||
        !streamer.writeFloat (humanize_) || !streamer.writeFloat (roomMix_))
        return kResultFalse;

    return kResultOk;
}

void CVBassFingerLiteProcessor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassTone, "Tone", "Tone", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.38f, 0.0f, 1.0f}, nullptr, 0, "tone", "%", "Bass", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassAttack, "Attack", "Attack", ParameterUnit::Milliseconds, ParameterScale::Linear, kParamFlags, {0.5f, 40.0f, 3.0f, 0.0f, 1.0f}, nullptr, 0, "attack", "ms", "Bass", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassVelocitySensitivity, "Velocity Sens", "Velocity Sensitivity", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.75f, 0.0f, 1.0f}, nullptr, 0, "velocity_sensitivity", "%", "Bass", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassOutputGain, "Output", "Output Gain", ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags, {-24.0f, 6.0f, -6.0f, 0.0f, 1.0f}, nullptr, 0, "output_gain", "dB", "Output", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassCompression, "Compression", "Compression", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.15f, 0.0f, 1.0f}, nullptr, 0, "compression", "%", "Mix", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassDrive, "Drive", "Drive", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "drive", "%", "Mix", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassLowEQ, "Bass", "Bass EQ", ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags, {-12.0f, 12.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "bass_eq", "dB", "EQ", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassMidEQ, "Mid", "Mid EQ", ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags, {-12.0f, 12.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "mid_eq", "dB", "EQ", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassHighEQ, "Treble", "Treble EQ", ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags, {-12.0f, 12.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "treble_eq", "dB", "EQ", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassFingerNoise, "Finger Noise", "Finger Noise", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "finger_noise", "%", "Humanize", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassHumanize, "Humanize", "Humanize", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.04f, 0.0f, 1.0f}, nullptr, 0, "humanize", "%", "Humanize", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBassRoom, "Room", "Room", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags, {0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "room", "%", "Room", 2), ParameterSmoothingMode::Linear);
}

void CVBassFingerLiteProcessor::applyParametersToDSP () noexcept
{
    tone_ = parameters_.getCurrentReal (kParamBassTone);
    attackMs_ = parameters_.getCurrentReal (kParamBassAttack);
    velocitySensitivity_ = parameters_.getCurrentReal (kParamBassVelocitySensitivity);
    outputGainDb_ = parameters_.getCurrentReal (kParamBassOutputGain);
    compression_ = parameters_.getCurrentReal (kParamBassCompression);
    drive_ = parameters_.getCurrentReal (kParamBassDrive);
    bassGainDb_ = parameters_.getCurrentReal (kParamBassLowEQ);
    midGainDb_ = parameters_.getCurrentReal (kParamBassMidEQ);
    trebleGainDb_ = parameters_.getCurrentReal (kParamBassHighEQ);
    fingerNoise_ = parameters_.getCurrentReal (kParamBassFingerNoise);
    humanize_ = parameters_.getCurrentReal (kParamBassHumanize);
    roomMix_ = parameters_.getCurrentReal (kParamBassRoom);

    bassVoice_.setTone (tone_);
    bassVoice_.setAttackMs (attackMs_);
    bassVoice_.setVelocitySensitivity (velocitySensitivity_);
    bassVoice_.setCompression (compression_);
    bassVoice_.setDrive (drive_);
    bassVoice_.setBassGainDb (bassGainDb_);
    bassVoice_.setMidGainDb (midGainDb_);
    bassVoice_.setTrebleGainDb (trebleGainDb_);
    bassVoice_.setFingerNoise (fingerNoise_);
    bassVoice_.setHumanize (humanize_);
    bassVoice_.setOutputGainDb (outputGainDb_);
    roomReverb_.setWet (roomMix_ * 0.22f);
}

std::size_t CVBassFingerLiteProcessor::collectMidiEvents (Vst::IEventList* inputEvents, int32 numSamples) noexcept
{
    if (!inputEvents)
        return 0;

    std::size_t count = 0;
    const auto blockSize = static_cast<std::size_t> (std::max<int32> (numSamples, 0));
    const int32 eventCount = inputEvents->getEventCount ();
    for (int32 eventIndex = 0; eventIndex < eventCount && count < kMaxMidiEventsPerBlock; ++eventIndex)
    {
        Vst::Event event {};
        if (inputEvents->getEvent (eventIndex, event) != kResultOk)
            continue;

        MidiEvent midiEvent {};
        midiEvent.sampleOffset = clampSampleOffset (event.sampleOffset, numSamples);

        switch (event.type)
        {
            case Vst::Event::kNoteOnEvent:
            {
                midiEvent.type = cvdsp::manager::MidiEventType::NoteOn;
                midiEvent.channel = clampToMidiChannel (event.noteOn.channel);
                midiEvent.note = clampToMidiByte (event.noteOn.pitch);
                midiEvent.value = std::clamp (event.noteOn.velocity, 0.0f, 1.0f);
                midiEvent.noteId = event.noteOn.noteId;
                pushMidiEventSorted (cvdsp::manager::sanitizeMidiEvent (midiEvent, blockSize), count);
                break;
            }

            case Vst::Event::kNoteOffEvent:
            {
                midiEvent.type = cvdsp::manager::MidiEventType::NoteOff;
                midiEvent.channel = clampToMidiChannel (event.noteOff.channel);
                midiEvent.note = clampToMidiByte (event.noteOff.pitch);
                midiEvent.value = std::clamp (event.noteOff.velocity, 0.0f, 1.0f);
                midiEvent.noteId = event.noteOff.noteId;
                pushMidiEventSorted (cvdsp::manager::sanitizeMidiEvent (midiEvent, blockSize), count);
                break;
            }

            default:
                break;
        }
    }

    return count;
}

void CVBassFingerLiteProcessor::pushMidiEventSorted (const MidiEvent& event, std::size_t& count) noexcept
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

void CVBassFingerLiteProcessor::applyMidiEventToVoice (const MidiEvent& event) noexcept
{
    const auto result = noteTracker_.handleMidiEvent (event);

    if (result.noteReleased)
    {
        bassVoice_.noteOff ();
        return;
    }

    if (result.noteStarted || result.noteChanged || result.isLegato)
        bassVoice_.noteOn (result.note, result.velocity, result.isLegato);
}

bool CVBassFingerLiteProcessor::processRoomOnMainOutput (Vst::ProcessData& data, std::size_t sampleCount) noexcept
{
    if (roomMix_ <= 0.0001f || sampleCount == 0 || data.numOutputs <= 0)
        return false;

    auto& mainOutput = data.outputs[0];
    const auto channelsToProcess = static_cast<std::size_t> (std::clamp<int32> (mainOutput.numChannels, 0, 2));
    if (channelsToProcess == 0 || !mainOutput.channelBuffers32)
        return false;

    std::array<float*, 2> channels {};
    for (std::size_t channel = 0; channel < channelsToProcess; ++channel)
    {
        channels[channel] = mainOutput.channelBuffers32[channel];
        if (!channels[channel])
            return false;
    }

    cvdsp::AudioBufferView<float> buffer (channels.data (), channelsToProcess, sampleCount);
    roomReverb_.processBlock (buffer);

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        for (std::size_t channel = 0; channel < channelsToProcess; ++channel)
        {
            if (std::abs (channels[channel][sample]) > 1.0e-8f)
                return true;
        }
    }

    return false;
}

float CVBassFingerLiteProcessor::renderSample () noexcept
{
    return bassVoice_.processSample ();
}

} // namespace CV
