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

DCBlockerVST3Processor::DCBlockerVST3Processor ()
{
    setControllerClass (kDCBlockerVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

DCBlockerVST3Processor::~DCBlockerVST3Processor ()
{}

tresult PLUGIN_API DCBlockerVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API DCBlockerVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API DCBlockerVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& dsp : processors_)
            dsp.reset ();
        
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API DCBlockerVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& dsp : processors_)
        dsp.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API DCBlockerVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API DCBlockerVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API DCBlockerVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float cutoff = 20.0f;
    if (!streamer.readFloat (cutoff))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamDCBlockerCutoff, cutoff);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API DCBlockerVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!(streamer.writeFloat (cutoffHz_)))
        return kResultFalse;
    return kResultOk;
}

void DCBlockerVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamDCBlockerCutoff, "Cutoff", "Cutoff", ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags, {1.0f, 200.0f, 20.0f, 0.0f, 1.0f}, nullptr, 0, "cutoff", "Hz", "DC Blocker", 2), ParameterSmoothingMode::Linear);
}

void DCBlockerVST3Processor::applyParametersToDSP () noexcept
{
    cutoffHz_ = parameters_.getCurrentReal (kParamDCBlockerCutoff);
    for (auto& dsp : processors_)
        dsp.setCutoffHz (cutoffHz_);
}

} // namespace CV
