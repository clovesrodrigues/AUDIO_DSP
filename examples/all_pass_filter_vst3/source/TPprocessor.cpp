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

AllPassFilterVST3Processor::AllPassFilterVST3Processor ()
{
    setControllerClass (kAllPassFilterVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

AllPassFilterVST3Processor::~AllPassFilterVST3Processor ()
{}

tresult PLUGIN_API AllPassFilterVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API AllPassFilterVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API AllPassFilterVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& dsp : processors_)
            dsp.reset ();
        
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API AllPassFilterVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& dsp : processors_)
        dsp.prepare (static_cast<float> (sampleRate_), 4096u);
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API AllPassFilterVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API AllPassFilterVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API AllPassFilterVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float delay = 128.0f;
    float feedback = 0.5f;
    if (!streamer.readFloat (delay) || !streamer.readFloat (feedback))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamAllPassDelaySamples, delay);
    (void)parameters_.setImmediateReal (kParamAllPassFeedback, feedback);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API AllPassFilterVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!(streamer.writeFloat (delaySamples_) && streamer.writeFloat (feedback_)))
        return kResultFalse;
    return kResultOk;
}

void AllPassFilterVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAllPassDelaySamples, "Delay", "Delay Samples", ParameterUnit::Samples, ParameterScale::Linear, kParamFlags, {1.0f, 4096.0f, 128.0f, 0.0f, 1.0f}, nullptr, 0, "delay_samples", "samples", "All Pass", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamAllPassFeedback, "Feedback", "Feedback", ParameterUnit::Percent, ParameterScale::Linear, kParamFlags, {-0.999f, 0.999f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "feedback", "", "All Pass", 2), ParameterSmoothingMode::Linear);
}

void AllPassFilterVST3Processor::applyParametersToDSP () noexcept
{
    delaySamples_ = parameters_.getCurrentReal (kParamAllPassDelaySamples);
    feedback_ = parameters_.getCurrentReal (kParamAllPassFeedback);
    const auto delaySamples = static_cast<std::size_t> (std::clamp (delaySamples_, 1.0f, 4096.0f));
    for (auto& dsp : processors_)
    {
        dsp.setDelaySamples (delaySamples);
        dsp.setFeedback (feedback_);
    }
}

} // namespace CV
