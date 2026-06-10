//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cstring>

using namespace Steinberg;

namespace CV {
namespace {
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;
}

AmpSimulatorVST3Processor::AmpSimulatorVST3Processor ()
{
    setControllerClass (kAmpSimulatorVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

AmpSimulatorVST3Processor::~AmpSimulatorVST3Processor ()
{}

tresult PLUGIN_API AmpSimulatorVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API AmpSimulatorVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API AmpSimulatorVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& processor : processors_)
            processor.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API AmpSimulatorVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& processor : processors_)
        (void)processor.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API AmpSimulatorVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API AmpSimulatorVST3Processor::process (Vst::ProcessData& data)
{
    if (data.symbolicSampleSize != Vst::kSample32)
        return kResultFalse;

    const auto sampleCount = static_cast<std::size_t> (std::max<int32> (data.numSamples, 0));
    parameters_.beginBlock (sampleCount);
    cvdsp::adapters::vst3::VST3ParameterAdapter::adaptParameterChanges (data.inputParameterChanges, parameters_);
    parameters_.processBlockParameters (sampleCount);
    applyParametersToDSP ();

    if (data.numSamples <= 0)
        return kResultOk;

    const int32 minBus = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < minBus; ++bus)
    {
        const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (processors_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = processSample (input[sample], static_cast<std::size_t> (channel));
        }

        for (int32 channel = processChan; channel < minChan; ++channel)
        {
            if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
            {
                std::memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel],
                             sampleCount * sizeof (Vst::Sample32));
            }
        }
        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }
    return kResultOk;
}

tresult PLUGIN_API AmpSimulatorVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float inputGain = 1.0f;
    float preampDrive = 4.0f;
    float bass = 0.5f;
    float mid = 0.5f;
    float treble = 0.5f;
    float presence = 0.5f;
    float powerModel = 0.0f;
    float outputGain = 1.0f;
    if (!streamer.readFloat (inputGain) || !streamer.readFloat (preampDrive) || !streamer.readFloat (bass) || !streamer.readFloat (mid) || !streamer.readFloat (treble) || !streamer.readFloat (presence) || !streamer.readFloat (powerModel) || !streamer.readFloat (outputGain))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamAmpSimulatorInputGain, inputGain);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorPreampDrive, preampDrive);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorBass, bass);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorMid, mid);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorTreble, treble);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorPresence, presence);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorPowerModel, powerModel);
    (void)parameters_.setImmediateReal (kParamAmpSimulatorOutputGain, outputGain);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API AmpSimulatorVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (inputGain_) || !streamer.writeFloat (preampDrive_) || !streamer.writeFloat (bass_) || !streamer.writeFloat (mid_) || !streamer.writeFloat (treble_) || !streamer.writeFloat (presence_) || !streamer.writeFloat (powerModel_) || !streamer.writeFloat (outputGain_))
        return kResultFalse;
    return kResultOk;
}

void AmpSimulatorVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorInputGain, "Input Gain", "Input Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 4.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "inputGain", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorPreampDrive, "Preamp Drive", "Preamp Drive",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 30.0f, 4.0f, 0.0f, 1.0f}, nullptr, 0, "preampDrive", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorBass, "Bass", "Bass",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "bass", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorMid, "Mid", "Mid",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "mid", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorTreble, "Treble", "Treble",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "treble", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorPresence, "Presence", "Presence",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "presence", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorPowerModel, "Power Model", "Power Model",
        ParameterUnit::Index, ParameterScale::Linear, kParamFlags,
        {0.0f, 3.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "powerModel", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAmpSimulatorOutputGain, "Output Gain", "Output Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 4.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "outputGain", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
}

void AmpSimulatorVST3Processor::applyParametersToDSP () noexcept
{
    inputGain_ = parameters_.getCurrentReal (kParamAmpSimulatorInputGain);
    preampDrive_ = parameters_.getCurrentReal (kParamAmpSimulatorPreampDrive);
    bass_ = parameters_.getCurrentReal (kParamAmpSimulatorBass);
    mid_ = parameters_.getCurrentReal (kParamAmpSimulatorMid);
    treble_ = parameters_.getCurrentReal (kParamAmpSimulatorTreble);
    presence_ = parameters_.getCurrentReal (kParamAmpSimulatorPresence);
    powerModel_ = parameters_.getCurrentReal (kParamAmpSimulatorPowerModel);
    outputGain_ = parameters_.getCurrentReal (kParamAmpSimulatorOutputGain);

    for (auto& processor : processors_)
    {
        processor.setInputGain (inputGain_);
        processor.setPreampDrive (preampDrive_);
        processor.setBass (bass_);
        processor.setMid (mid_);
        processor.setTreble (treble_);
        processor.setPresence (presence_);
        processor.setPowerAmpModel (toPowerAmpModel (powerModel_));
        processor.setOutputGain (outputGain_);
        processor.setNoiseGateEnabled (false);
        processor.setCabinetEnabled (false);
    }
}

float AmpSimulatorVST3Processor::processSample (float input, std::size_t channel) noexcept
{
    auto& processor = processors_[channel];
    return processor.process (input);
}

cvdsp::PowerAmp<float>::Model AmpSimulatorVST3Processor::toPowerAmpModel (float plain) noexcept
{
    const auto index = std::clamp (static_cast<int> (plain + 0.5f), 0, 3);
    switch (index)
    {
        case 1: return cvdsp::PowerAmp<float>::Model::SixL6;
        case 2: return cvdsp::PowerAmp<float>::Model::KT88;
        case 3: return cvdsp::PowerAmp<float>::Model::EL84;
        default: return cvdsp::PowerAmp<float>::Model::EL34;
    }
}

} // namespace CV
