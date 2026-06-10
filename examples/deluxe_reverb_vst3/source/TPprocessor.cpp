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
}

DeluxeReverbVST3Processor::DeluxeReverbVST3Processor ()
{
    setControllerClass (kDeluxeReverbVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

DeluxeReverbVST3Processor::~DeluxeReverbVST3Processor ()
{}

tresult PLUGIN_API DeluxeReverbVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API DeluxeReverbVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API DeluxeReverbVST3Processor::setActive (TBool state)
{
    if (state)
        resetDSP ();

    return AudioEffect::setActive (state);
}

tresult PLUGIN_API DeluxeReverbVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
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

tresult PLUGIN_API DeluxeReverbVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API DeluxeReverbVST3Processor::process (Vst::ProcessData& data)
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

tresult PLUGIN_API DeluxeReverbVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float mix = 0.25f;
    float dwell = 0.50f;
    float tone = 0.60f;

    if (!streamer.readFloat (mix) || !streamer.readFloat (dwell) || !streamer.readFloat (tone))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamMix, mix);
    (void)parameters_.setImmediateReal (kParamDwell, dwell);
    (void)parameters_.setImmediateReal (kParamTone, tone);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API DeluxeReverbVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (mix_) || !streamer.writeFloat (dwell_) || !streamer.writeFloat (tone_))
        return kResultFalse;

    return kResultOk;
}

void DeluxeReverbVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamMix, "Mix", "Wet/Dry Mix", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.25f}, nullptr, 0, "mix", "%", "Deluxe Reverb", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamDwell, "Dwell", "Tank Dwell", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.50f}, nullptr, 0, "dwell", "%", "Deluxe Reverb", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamTone, "Tone", "Tank Tone", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.60f}, nullptr, 0, "tone", "%", "Deluxe Reverb", 2));
}

void DeluxeReverbVST3Processor::prepareDSP () noexcept
{
    dsp_.prepare (dspContext_);
}

void DeluxeReverbVST3Processor::resetDSP () noexcept
{
    dsp_.reset ();
}

void DeluxeReverbVST3Processor::applyParametersToDSP () noexcept
{
    mix_ = clamp01 (parameters_.getCurrentReal (kParamMix));
    dwell_ = clamp01 (parameters_.getCurrentReal (kParamDwell));
    tone_ = clamp01 (parameters_.getCurrentReal (kParamTone));

    dsp_.setMix (mix_);
    dsp_.setDwell (dwell_);
    dsp_.setTone (tone_);
    dsp_.setReverbAmount (std::max (0.10f, dwell_));
}

} // namespace CV
