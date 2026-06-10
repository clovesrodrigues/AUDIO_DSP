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

MidSideVST3Processor::MidSideVST3Processor ()
{
    setControllerClass (kMidSideVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

MidSideVST3Processor::~MidSideVST3Processor ()
{}

tresult PLUGIN_API MidSideVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API MidSideVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API MidSideVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API MidSideVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API MidSideVST3Processor::process (Vst::ProcessData& data)
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
        const int32 inputChannels = data.inputs[bus].numChannels;
        const int32 outputChannels = data.outputs[bus].numChannels;
        const int32 minChannels = std::min (inputChannels, outputChannels);

        if (minChannels >= 2)
        {
            processStereoBlock (data.inputs[bus].channelBuffers32[0],
                                data.inputs[bus].channelBuffers32[1],
                                data.outputs[bus].channelBuffers32[0],
                                data.outputs[bus].channelBuffers32[1],
                                data.numSamples);

            for (int32 channel = 2; channel < minChannels; ++channel)
            {
                if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
                {
                    std::memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel],
                                 sampleCount * sizeof (Vst::Sample32));
                }
            }
        }
        else if (minChannels == 1)
        {
            if (data.outputs[bus].channelBuffers32[0] != data.inputs[bus].channelBuffers32[0])
            {
                std::memcpy (data.outputs[bus].channelBuffers32[0], data.inputs[bus].channelBuffers32[0],
                             sampleCount * sizeof (Vst::Sample32));
            }
        }

        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }
    return kResultOk;
}

tresult PLUGIN_API MidSideVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float midGain = 1.0f;
    float sideGain = 1.0f;
    float width = 1.0f;
    if (!streamer.readFloat (midGain) || !streamer.readFloat (sideGain) || !streamer.readFloat (width))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamMidSideVST3MidGain, midGain);
    (void)parameters_.setImmediateReal (kParamMidSideVST3SideGain, sideGain);
    (void)parameters_.setImmediateReal (kParamMidSideVST3Width, width);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API MidSideVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (midGain_) || !streamer.writeFloat (sideGain_) || !streamer.writeFloat (width_))
        return kResultFalse;
    return kResultOk;
}

void MidSideVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamMidSideVST3MidGain, "Mid Gain", "Mid Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 2.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "mid_gain", "", "Spatial", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamMidSideVST3SideGain, "Side Gain", "Side Gain",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 2.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "side_gain", "", "Spatial", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamMidSideVST3Width, "Stereo Width", "Stereo Width",
        ParameterUnit::None, ParameterScale::Linear, kParamFlags,
        {0.0f, 2.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "stereo_width", "", "Spatial", 2),
        ParameterSmoothingMode::Linear);
}

void MidSideVST3Processor::applyParametersToDSP () noexcept
{
    midGain_ = parameters_.getCurrentReal (kParamMidSideVST3MidGain);
    sideGain_ = parameters_.getCurrentReal (kParamMidSideVST3SideGain);
    width_ = parameters_.getCurrentReal (kParamMidSideVST3Width);
}

void MidSideVST3Processor::processStereoBlock (const Vst::Sample32* leftIn,
                                                const Vst::Sample32* rightIn,
                                                Vst::Sample32* leftOut,
                                                Vst::Sample32* rightOut,
                                                int32 sampleCount) noexcept
{
    for (int32 sample = 0; sample < sampleCount; ++sample)
    {
        const auto ms = DSP::encode (leftIn[sample], rightIn[sample]);
        const auto lr = DSP::decode (ms.mid * midGain_, ms.side * sideGain_ * width_);
        leftOut[sample] = lr.left;
        rightOut[sample] = lr.right;
    }
}

} // namespace CV
