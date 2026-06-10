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

TubePreampVST3Processor::TubePreampVST3Processor ()
{
    setControllerClass (kTubePreampVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

TubePreampVST3Processor::~TubePreampVST3Processor ()
{}

tresult PLUGIN_API TubePreampVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API TubePreampVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API TubePreampVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& processor : processors_)
            processor.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API TubePreampVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& processor : processors_)
        (void)processor.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API TubePreampVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API TubePreampVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API TubePreampVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float drive = 4.0f;
    float bias = 0.0f;
    float plateVoltage = 250.0f;
    float stages = 2.0f;
    float outputGain = 1.0f;
    if (!streamer.readFloat (drive) || !streamer.readFloat (bias) || !streamer.readFloat (plateVoltage) || !streamer.readFloat (stages) || !streamer.readFloat (outputGain))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamTubePreampDrive, drive);
    (void)parameters_.setImmediateReal (kParamTubePreampBias, bias);
    (void)parameters_.setImmediateReal (kParamTubePreampPlateVoltage, plateVoltage);
    (void)parameters_.setImmediateReal (kParamTubePreampStages, stages);
    (void)parameters_.setImmediateReal (kParamTubePreampOutputGain, outputGain);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API TubePreampVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (drive_) || !streamer.writeFloat (bias_) || !streamer.writeFloat (plateVoltage_) || !streamer.writeFloat (stages_) || !streamer.writeFloat (outputGain_))
        return kResultFalse;
    return kResultOk;
}

void TubePreampVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubePreampDrive, "Drive", "Drive",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 30.0f, 4.0f, 0.0f, 1.0f}, nullptr, 0, "drive", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubePreampBias, "Bias", "Bias",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {-1.0f, 1.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "bias", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubePreampPlateVoltage, "Plate Voltage", "Plate Voltage",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {80.0f, 400.0f, 250.0f, 0.0f, 1.0f}, nullptr, 0, "plateVoltage", "V", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubePreampStages, "Stages", "Stages",
        ParameterUnit::Index, ParameterScale::Linear, kParamFlags,
        {1.0f, 4.0f, 2.0f, 0.0f, 1.0f}, nullptr, 0, "stages", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamTubePreampOutputGain, "Output Gain", "Output Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 4.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "outputGain", "", "Guitar", 2),
        ParameterSmoothingMode::Linear);
}

void TubePreampVST3Processor::applyParametersToDSP () noexcept
{
    drive_ = parameters_.getCurrentReal (kParamTubePreampDrive);
    bias_ = parameters_.getCurrentReal (kParamTubePreampBias);
    plateVoltage_ = parameters_.getCurrentReal (kParamTubePreampPlateVoltage);
    stages_ = parameters_.getCurrentReal (kParamTubePreampStages);
    outputGain_ = parameters_.getCurrentReal (kParamTubePreampOutputGain);

    for (auto& processor : processors_)
    {
        processor.setDrive (drive_);
        processor.setBias (bias_);
        processor.setPlateVoltage (plateVoltage_);
        processor.setNumStages (toStageCount (stages_));
        processor.setOutputGain (outputGain_);
    }
}

float TubePreampVST3Processor::processSample (float input, std::size_t channel) noexcept
{
    auto& processor = processors_[channel];
    return processor.process (input);
}

std::size_t TubePreampVST3Processor::toStageCount (float plain) noexcept
{
    const auto rounded = static_cast<int> (plain + 0.5f);
    return static_cast<std::size_t> (std::clamp (rounded, 1, 4));
}

} // namespace CV
