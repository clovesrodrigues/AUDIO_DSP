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

PowerAmpVST3Processor::PowerAmpVST3Processor ()
{
    setControllerClass (kPowerAmpVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

PowerAmpVST3Processor::~PowerAmpVST3Processor ()
{}

tresult PLUGIN_API PowerAmpVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API PowerAmpVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API PowerAmpVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& processor : processors_)
            processor.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API PowerAmpVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& processor : processors_)
        (void)processor.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API PowerAmpVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API PowerAmpVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API PowerAmpVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float inputGain = 1.0f;
    float model = 0.0f;
    float outputGain = 1.0f;
    if (!streamer.readFloat (inputGain) || !streamer.readFloat (model) || !streamer.readFloat (outputGain))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamPowerAmpInputGain, inputGain);
    (void)parameters_.setImmediateReal (kParamPowerAmpModel, model);
    (void)parameters_.setImmediateReal (kParamPowerAmpOutputGain, outputGain);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API PowerAmpVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (inputGain_) || !streamer.writeFloat (model_) || !streamer.writeFloat (outputGain_))
        return kResultFalse;
    return kResultOk;
}

void PowerAmpVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamPowerAmpInputGain, "Input Gain", "Input Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 4.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "inputGain", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamPowerAmpModel, "Model", "Model",
        ParameterUnit::Index, ParameterScale::Linear, kParamFlags,
        {0.0f, 3.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "model", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamPowerAmpOutputGain, "Output Gain", "Output Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 4.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "outputGain", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
}

void PowerAmpVST3Processor::applyParametersToDSP () noexcept
{
    inputGain_ = parameters_.getCurrentReal (kParamPowerAmpInputGain);
    model_ = parameters_.getCurrentReal (kParamPowerAmpModel);
    outputGain_ = parameters_.getCurrentReal (kParamPowerAmpOutputGain);

    for (auto& processor : processors_)
    {
        processor.setModel (toPowerAmpModel (model_));
    }
}

float PowerAmpVST3Processor::processSample (float input, std::size_t channel) noexcept
{
    auto& processor = processors_[channel];
    const float wet = processor.process (input * inputGain_);
    return wet * outputGain_;
}

cvdsp::PowerAmp<float>::Model PowerAmpVST3Processor::toPowerAmpModel (float plain) noexcept
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
