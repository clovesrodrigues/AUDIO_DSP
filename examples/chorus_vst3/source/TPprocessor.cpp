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

ChorusVST3Processor::ChorusVST3Processor ()
{
    setControllerClass (kChorusVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

ChorusVST3Processor::~ChorusVST3Processor ()
{}

tresult PLUGIN_API ChorusVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API ChorusVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API ChorusVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& chorus : choruses_)
            chorus.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API ChorusVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& chorus : choruses_)
        chorus.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API ChorusVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API ChorusVST3Processor::process (Vst::ProcessData& data)
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
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (choruses_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& chorus = choruses_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = chorus.process (input[sample]);
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

tresult PLUGIN_API ChorusVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float rate = 0.5f;
    float depth = 0.5f;
    float mix = 0.5f;
    if (!streamer.readFloat (rate) || !streamer.readFloat (depth) || !streamer.readFloat (mix))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamChorusRate, rate);
    (void)parameters_.setImmediateReal (kParamChorusDepth, depth);
    (void)parameters_.setImmediateReal (kParamChorusMix, mix);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API ChorusVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (rateHz_) || !streamer.writeFloat (depth_) || !streamer.writeFloat (mix_))
        return kResultFalse;
    return kResultOk;
}

void ChorusVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamChorusRate, "Rate", "Rate",
        ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags,
        {0.01f, 20.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "rate", "Hz", "Chorus", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamChorusDepth, "Depth", "Depth",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "depth", "", "Chorus", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamChorusMix, "Mix", "Mix",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 0.5f, 0.0f, 1.0f}, nullptr, 0, "mix", "", "Chorus", 2),
        ParameterSmoothingMode::Linear);
}

void ChorusVST3Processor::applyParametersToDSP () noexcept
{
    rateHz_ = parameters_.getCurrentReal (kParamChorusRate);
    depth_ = parameters_.getCurrentReal (kParamChorusDepth);
    mix_ = parameters_.getCurrentReal (kParamChorusMix);

    for (auto& chorus : choruses_)
    {
        chorus.setRate (rateHz_);
        chorus.setDepth (depth_);
        chorus.setMix (mix_);
    }
}

} // namespace CV
