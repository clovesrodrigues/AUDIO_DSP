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

ADSRVST3Processor::ADSRVST3Processor ()
{
    setControllerClass (kADSRVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

ADSRVST3Processor::~ADSRVST3Processor () {}

tresult PLUGIN_API ADSRVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API ADSRVST3Processor::terminate () { return AudioEffect::terminate (); }

tresult PLUGIN_API ADSRVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& env : envelopes_)
            env.reset ();
        gateActive_ = false;
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API ADSRVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& env : envelopes_)
        env.prepare (static_cast<float> (sampleRate_), attackMs_, decayMs_, sustain_, releaseMs_);
    gateActive_ = false;
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API ADSRVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API ADSRVST3Processor::process (Vst::ProcessData& data)
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
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (envelopes_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& env = envelopes_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = input[sample] * env.process ();
        }

        for (int32 channel = processChan; channel < minChan; ++channel)
        {
            if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
                std::memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel], sampleCount * sizeof (Vst::Sample32));
        }
        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }
    return kResultOk;
}

tresult PLUGIN_API ADSRVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float gate = 0.0f, attack = 10.0f, decay = 100.0f, sustain = 0.7f, release = 250.0f;
    if (!streamer.readFloat (gate) || !streamer.readFloat (attack) || !streamer.readFloat (decay) || !streamer.readFloat (sustain) || !streamer.readFloat (release))
        return kResultFalse;
    (void)parameters_.setImmediateReal (kParamADSRGate, gate);
    (void)parameters_.setImmediateReal (kParamADSRAttack, attack);
    (void)parameters_.setImmediateReal (kParamADSRDecay, decay);
    (void)parameters_.setImmediateReal (kParamADSRSustain, sustain);
    (void)parameters_.setImmediateReal (kParamADSRRelease, release);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API ADSRVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (gate_) || !streamer.writeFloat (attackMs_) || !streamer.writeFloat (decayMs_) || !streamer.writeFloat (sustain_) || !streamer.writeFloat (releaseMs_))
        return kResultFalse;
    return kResultOk;
}

void ADSRVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamADSRGate, "Gate", "Gate", ParameterUnit::None, ParameterScale::Boolean, kParamFlags, {0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "gate", "", "ADSR", 2), ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamADSRAttack, "Attack", "Attack", ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags, {0.1f, 5000.0f, 10.0f, 0.0f, 1.0f}, nullptr, 0, "attack", "ms", "ADSR", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamADSRDecay, "Decay", "Decay", ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags, {0.1f, 5000.0f, 100.0f, 0.0f, 1.0f}, nullptr, 0, "decay", "ms", "ADSR", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamADSRSustain, "Sustain", "Sustain", ParameterUnit::Percent, ParameterScale::Linear, kParamFlags, {0.0f, 1.0f, 0.7f, 0.0f, 1.0f}, nullptr, 0, "sustain", "", "ADSR", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamADSRRelease, "Release", "Release", ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags, {0.1f, 10000.0f, 250.0f, 0.0f, 1.0f}, nullptr, 0, "release", "ms", "ADSR", 2), ParameterSmoothingMode::Linear);
}

void ADSRVST3Processor::applyParametersToDSP () noexcept
{
    const bool newGateActive = parameters_.getCurrentReal (kParamADSRGate) >= 0.5f;
    gate_ = newGateActive ? 1.0f : 0.0f;
    attackMs_ = parameters_.getCurrentReal (kParamADSRAttack);
    decayMs_ = parameters_.getCurrentReal (kParamADSRDecay);
    sustain_ = parameters_.getCurrentReal (kParamADSRSustain);
    releaseMs_ = parameters_.getCurrentReal (kParamADSRRelease);

    for (auto& env : envelopes_)
    {
        env.setAttack (attackMs_);
        env.setDecay (decayMs_);
        env.setSustain (sustain_);
        env.setRelease (releaseMs_);
        if (newGateActive != gateActive_)
            newGateActive ? env.noteOn () : env.noteOff ();
    }
    gateActive_ = newGateActive;
}

} // namespace CV
