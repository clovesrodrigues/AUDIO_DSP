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

float clamp01 (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

float denormalize (float normalized, float minPlain, float maxPlain) noexcept
{
    const float clamped = clamp01 (normalized);
    return minPlain + (clamped * (maxPlain - minPlain));
}
}

PlateReverbVST3Processor::PlateReverbVST3Processor ()
{
    setControllerClass (kPlateReverbVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

PlateReverbVST3Processor::~PlateReverbVST3Processor ()
{}

tresult PLUGIN_API PlateReverbVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API PlateReverbVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API PlateReverbVST3Processor::setActive (TBool state)
{
    if (state)
        resetDSP ();

    return AudioEffect::setActive (state);
}

tresult PLUGIN_API PlateReverbVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;

    dspContext_.sampleRate = static_cast<float> (sampleRate_);
    dspContext_.blockSize = static_cast<std::size_t> (std::max<int32> (newSetup.maxSamplesPerBlock, 0));
    dspContext_.numChannels = 2;

    (void)parameters_.prepare (dspContext_);
    prepareDSP ();
    applyParametersToDSP ();

    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API PlateReverbVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API PlateReverbVST3Processor::process (Vst::ProcessData& data)
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

    const int32 busCount = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < busCount; ++bus)
    {
        const int32 channelCount = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChannels = std::min<int32> (channelCount, 2);

        for (int32 channel = 0; channel < channelCount; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            if (input && output && input != output)
                std::memcpy (output, input, sampleCount * sizeof (Vst::Sample32));
        }

        if (processChannels > 0)
        {
            float* channelPointers[2] {
                data.outputs[bus].channelBuffers32[0],
                processChannels > 1 ? data.outputs[bus].channelBuffers32[1] : nullptr
            };

            cvdsp::AudioBufferView<float> view (
                channelPointers,
                static_cast<std::size_t> (processChannels),
                sampleCount);

            dsp_.processBlock (view);
        }

        data.outputs[bus].silenceFlags = 0;
    }

    return kResultOk;
}

tresult PLUGIN_API PlateReverbVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mix = 0.25f;
    float decay = 0.50f;
    float damping = 0.35f;
    float preDelay = 20.0f;

    if (!streamer.readFloat (mix) || !streamer.readFloat (decay) || !streamer.readFloat (damping) ||
        !streamer.readFloat (preDelay))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamMix, mix);
    (void)parameters_.setImmediateReal (kParamDecay, decay);
    (void)parameters_.setImmediateReal (kParamDamping, damping);
    (void)parameters_.setImmediateReal (kParamPreDelay, preDelay);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API PlateReverbVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (mix_) || !streamer.writeFloat (decay_) || !streamer.writeFloat (damping_) ||
        !streamer.writeFloat (preDelayMs_))
        return kResultFalse;

    return kResultOk;
}

void PlateReverbVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamMix, "Mix", "Wet/Dry Mix", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.25f}, nullptr, 0, "mix", "%", "Plate Reverb", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamDecay, "Decay", "Decay", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.50f}, nullptr, 0, "decay", "%", "Plate Reverb", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamDamping, "Damping", "High-Frequency Damping", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.35f}, nullptr, 0, "damping", "%", "Plate Reverb", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamPreDelay, "PreDelay", "Pre-Delay", ParameterUnit::Milliseconds, ParameterScale::Linear, kParamFlags,
        ParameterRange<float> {0.0f, 250.0f, 20.0f}, nullptr, 0, "predelay", "ms", "Plate Reverb", 1));
}

void PlateReverbVST3Processor::prepareDSP () noexcept
{
    dsp_.prepare (dspContext_);
}

void PlateReverbVST3Processor::resetDSP () noexcept
{
    dsp_.reset ();
}

void PlateReverbVST3Processor::applyParametersToDSP () noexcept
{
    mix_ = clamp01 (parameters_.getCurrentReal (kParamMix));
    decay_ = clamp01 (parameters_.getCurrentReal (kParamDecay));
    damping_ = clamp01 (parameters_.getCurrentReal (kParamDamping));
    preDelayMs_ = std::clamp (parameters_.getCurrentReal (kParamPreDelay), 0.0f, 250.0f);

    dsp_.setWet (mix_);
    dsp_.setDry (1.0f - mix_);
    dsp_.setDecay (decay_);
    dsp_.setDamping (damping_);
    dsp_.setPreDelay (std::min (preDelayMs_, 120.0f));
    dsp_.setBrightness (1.0f - damping_);
}

} // namespace CV
