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

TubeSatVST3Processor::TubeSatVST3Processor ()
{
    setControllerClass (kTubeSatVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

TubeSatVST3Processor::~TubeSatVST3Processor ()
{}

tresult PLUGIN_API TubeSatVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API TubeSatVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API TubeSatVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& processor : processors_)
            processor.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API TubeSatVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& processor : processors_)
        processor.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API TubeSatVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API TubeSatVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API TubeSatVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float drive = 2.0f;
    float bias = 0.0f;
    float mix = 1.0f;
    if (!streamer.readFloat (drive) || !streamer.readFloat (bias) || !streamer.readFloat (mix))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamTubeSatDrive, drive);
    (void)parameters_.setImmediateReal (kParamTubeSatBias, bias);
    (void)parameters_.setImmediateReal (kParamTubeSatMix, mix);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API TubeSatVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (drive_) || !streamer.writeFloat (bias_) || !streamer.writeFloat (mix_))
        return kResultFalse;
    return kResultOk;
}

void TubeSatVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubeSatDrive, "Drive", "Drive",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {1.0f, 50.0f, 2.0f, 0.0f, 1.0f}, nullptr, 0, "drive", "", "Saturation", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubeSatBias, "Bias", "Bias",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {-1.0f, 1.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "bias", "", "Saturation", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubeSatMix, "Mix", "Mix",
        ParameterUnit::Percent, ParameterScale::Linear, kParamFlags,
        {0.0f, 1.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "mix", "", "Saturation", 2),
        ParameterSmoothingMode::Linear);
}

void TubeSatVST3Processor::applyParametersToDSP () noexcept
{
    drive_ = parameters_.getCurrentReal (kParamTubeSatDrive);
    bias_ = parameters_.getCurrentReal (kParamTubeSatBias);
    outputScale_ = 1.0f / (1.0f + (0.03f * (drive_ - 1.0f)));
    mix_ = parameters_.getCurrentReal (kParamTubeSatMix);

    for (auto& processor : processors_)
    {
        processor.setDrive (drive_);
        processor.setBias (bias_);
        processor.setOutputGain (1.0f);
    }
}

float TubeSatVST3Processor::processSample (float input, std::size_t channel) noexcept
{
    auto& processor = processors_[channel];
    const float wet = processor.process (input);
    const float mixed = (input * (1.0f - mix_)) + (wet * mix_);
    return std::clamp (mixed * outputScale_, -0.98f, 0.98f);
}

} // namespace CV
