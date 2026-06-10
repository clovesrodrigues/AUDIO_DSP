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

LFOVST3Processor::LFOVST3Processor ()
{
    setControllerClass (kLFOVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

LFOVST3Processor::~LFOVST3Processor () {}

tresult PLUGIN_API LFOVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API LFOVST3Processor::terminate () { return AudioEffect::terminate (); }

tresult PLUGIN_API LFOVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& lfo : lfos_)
            lfo.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API LFOVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& lfo : lfos_)
        lfo.prepare (static_cast<float> (sampleRate_), frequencyHz_, 1.0f, cvdsp::modulation::LFOWaveform::Sine);
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API LFOVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API LFOVST3Processor::process (Vst::ProcessData& data)
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
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (lfos_.size ()));
        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& lfo = lfos_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
            {
                const float modulation = (lfo.process () + 1.0f) * 0.5f;
                output[sample] = input[sample] * modulation;
            }
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

tresult PLUGIN_API LFOVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float waveform = 0.0f, frequency = 1.0f;
    if (!streamer.readFloat (waveform) || !streamer.readFloat (frequency))
        return kResultFalse;
    (void)parameters_.setImmediateReal (kParamLFOWaveform, waveform);
    (void)parameters_.setImmediateReal (kParamLFOFrequency, frequency);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API LFOVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (waveformIndex_) || !streamer.writeFloat (frequencyHz_))
        return kResultFalse;
    return kResultOk;
}

void LFOVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamLFOWaveform, "Waveform", "Waveform", ParameterUnit::None, ParameterScale::Enum, kParamFlags, {0.0f, 3.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "waveform", "", "LFO", 2), ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamLFOFrequency, "Frequency", "Frequency", ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags, {0.01f, 50.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "frequency", "Hz", "LFO", 2), ParameterSmoothingMode::Linear);
}

void LFOVST3Processor::applyParametersToDSP () noexcept
{
    waveformIndex_ = parameters_.getCurrentReal (kParamLFOWaveform);
    frequencyHz_ = parameters_.getCurrentReal (kParamLFOFrequency);
    const int wave = std::clamp (static_cast<int> (waveformIndex_ + 0.5f), 0, 3);
    const auto waveform = static_cast<cvdsp::modulation::LFOWaveform> (wave);
    for (auto& lfo : lfos_)
    {
        lfo.setWaveform (waveform);
        lfo.setRate (frequencyHz_);
        lfo.setDepth (1.0f);
    }
}

} // namespace CV
