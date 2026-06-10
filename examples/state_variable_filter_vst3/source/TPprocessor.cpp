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

StateVariableFilterVST3Processor::StateVariableFilterVST3Processor ()
{
    setControllerClass (kStateVariableFilterVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

StateVariableFilterVST3Processor::~StateVariableFilterVST3Processor ()
{}

tresult PLUGIN_API StateVariableFilterVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API StateVariableFilterVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API StateVariableFilterVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& dsp : processors_)
            dsp.reset ();
        
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API StateVariableFilterVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& dsp : processors_)
        dsp.prepare (static_cast<float> (sampleRate_), cvdsp::filters::SVFMode::LowPass);
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API StateVariableFilterVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API StateVariableFilterVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API StateVariableFilterVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mode = 0.0f;
    float cutoff = 1000.0f;
    float resonance = 0.707f;
    if (!streamer.readFloat (mode) || !streamer.readFloat (cutoff) || !streamer.readFloat (resonance))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamSVFMode, mode);
    (void)parameters_.setImmediateReal (kParamSVFCutoff, cutoff);
    (void)parameters_.setImmediateReal (kParamSVFResonance, resonance);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API StateVariableFilterVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!(streamer.writeFloat (modeIndex_) && streamer.writeFloat (cutoffHz_) && streamer.writeFloat (resonance_)))
        return kResultFalse;
    return kResultOk;
}

void StateVariableFilterVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamSVFMode, "Mode", "Mode", ParameterUnit::None, ParameterScale::Enum, kParamFlags, {0.0f, 3.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "mode", "", "SVF", 2), ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamSVFCutoff, "Cutoff", "Cutoff", ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags, {20.0f, 20000.0f, 1000.0f, 0.0f, 1.0f}, nullptr, 0, "cutoff", "Hz", "SVF", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamSVFResonance, "Res", "Resonance", ParameterUnit::None, ParameterScale::Logarithmic, kParamFlags, {0.1f, 40.0f, 0.707f, 0.0f, 1.0f}, nullptr, 0, "resonance", "Q", "SVF", 2), ParameterSmoothingMode::Linear);
}

void StateVariableFilterVST3Processor::applyParametersToDSP () noexcept
{
    modeIndex_ = parameters_.getCurrentReal (kParamSVFMode);
    cutoffHz_ = parameters_.getCurrentReal (kParamSVFCutoff);
    resonance_ = parameters_.getCurrentReal (kParamSVFResonance);
    const int mode = std::clamp (static_cast<int> (modeIndex_ + 0.5f), 0, 3);
    const auto svfMode = static_cast<cvdsp::filters::SVFMode> (mode);
    for (auto& dsp : processors_)
    {
        dsp.setMode (svfMode);
        dsp.setCutoff (cutoffHz_);
        dsp.setResonance (resonance_);
    }
}

} // namespace CV
