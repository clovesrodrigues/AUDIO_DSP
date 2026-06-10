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

BiquadFilterVST3Processor::BiquadFilterVST3Processor ()
{
    setControllerClass (kBiquadFilterVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

BiquadFilterVST3Processor::~BiquadFilterVST3Processor ()
{}

tresult PLUGIN_API BiquadFilterVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API BiquadFilterVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API BiquadFilterVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& dsp : processors_)
            dsp.reset ();
        
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API BiquadFilterVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& dsp : processors_)
        dsp.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API BiquadFilterVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API BiquadFilterVST3Processor::process (Vst::ProcessData& data)
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
            auto& dsp = processors_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = dsp.process (input[sample]);
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

tresult PLUGIN_API BiquadFilterVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mode = 0.0f;
    float freq = 1000.0f;
    float q = 0.707f;
    float gain = 0.0f;
    if (!streamer.readFloat (mode) || !streamer.readFloat (freq) || !streamer.readFloat (q) || !streamer.readFloat (gain))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamBiquadMode, mode);
    (void)parameters_.setImmediateReal (kParamBiquadFrequency, freq);
    (void)parameters_.setImmediateReal (kParamBiquadQ, q);
    (void)parameters_.setImmediateReal (kParamBiquadGain, gain);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API BiquadFilterVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!(streamer.writeFloat (modeIndex_) && streamer.writeFloat (frequencyHz_) && streamer.writeFloat (q_) && streamer.writeFloat (gainDB_)))
        return kResultFalse;
    return kResultOk;
}

void BiquadFilterVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBiquadMode, "Mode", "Mode", ParameterUnit::None, ParameterScale::Enum, kParamFlags, {0.0f, 7.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "mode", "", "Biquad", 2), ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBiquadFrequency, "Freq", "Frequency", ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags, {20.0f, 20000.0f, 1000.0f, 0.0f, 1.0f}, nullptr, 0, "frequency", "Hz", "Biquad", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBiquadQ, "Q", "Q", ParameterUnit::None, ParameterScale::Logarithmic, kParamFlags, {0.1f, 20.0f, 0.707f, 0.0f, 1.0f}, nullptr, 0, "q", "", "Biquad", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamBiquadGain, "Gain", "Gain", ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags, {-24.0f, 24.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "gain", "dB", "Biquad", 2), ParameterSmoothingMode::Linear);
}

void BiquadFilterVST3Processor::applyParametersToDSP () noexcept
{
    modeIndex_ = parameters_.getCurrentReal (kParamBiquadMode);
    frequencyHz_ = parameters_.getCurrentReal (kParamBiquadFrequency);
    q_ = parameters_.getCurrentReal (kParamBiquadQ);
    gainDB_ = parameters_.getCurrentReal (kParamBiquadGain);
    const int mode = std::clamp (static_cast<int> (modeIndex_ + 0.5f), 0, 7);
    const auto type = static_cast<cvdsp::filters::BiquadType> (mode);
    for (auto& dsp : processors_)
    {
        dsp.setType (type);
        dsp.setFrequency (frequencyHz_);
        dsp.setQ (q_);
        dsp.setGainDB (gainDB_);
        dsp.updateCoefficients ();
    }
}

} // namespace CV
