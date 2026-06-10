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

ExpanderVST3Processor::ExpanderVST3Processor ()
{
    setControllerClass (kExpanderVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

ExpanderVST3Processor::~ExpanderVST3Processor ()
{}

tresult PLUGIN_API ExpanderVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API ExpanderVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API ExpanderVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& expander : expanders_)
            expander.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API ExpanderVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : sampleRate_;
    dspPrepared_ = false;

    cvdsp::manager::ParameterSmoothingConfig<float> smoothing {};
    smoothing.sampleRate = static_cast<float> (sampleRate_);
    smoothing.rampTimeSeconds = 0.010f;
    (void)parameters_.prepare (static_cast<float> (sampleRate_), static_cast<std::size_t> (newSetup.maxSamplesPerBlock), smoothing);
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API ExpanderVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API ExpanderVST3Processor::process (Vst::ProcessData& data)
{
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
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (expanders_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& expander = expanders_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = expander.process (input[sample]);
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

tresult PLUGIN_API ExpanderVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float threshold = -40.0f;
    float ratio = 4.0f;
    float attack = 5.0f;
    float release = 100.0f;
    if (!streamer.readFloat (threshold) || !streamer.readFloat (ratio) ||
        !streamer.readFloat (attack) || !streamer.readFloat (release))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamExpanderThreshold, threshold);
    (void)parameters_.setImmediateReal (kParamExpanderRatio, ratio);
    (void)parameters_.setImmediateReal (kParamExpanderAttack, attack);
    (void)parameters_.setImmediateReal (kParamExpanderRelease, release);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API ExpanderVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (thresholdDB_) || !streamer.writeFloat (ratio_) ||
        !streamer.writeFloat (attackMs_) || !streamer.writeFloat (releaseMs_))
        return kResultFalse;
    return kResultOk;
}

void ExpanderVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamExpanderThreshold, "Thresh", "Threshold",
        ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
        {-80.0f, 0.0f, -40.0f, 0.0f, 1.0f}, nullptr, 0, "threshold", "dB", "Expander", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamExpanderRatio, "Ratio", "Ratio",
        ParameterUnit::Ratio, ParameterScale::Linear, kParamFlags,
        {1.0f, 20.0f, 4.0f, 0.0f, 1.0f}, nullptr, 0, "ratio", ":1", "Expander", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamExpanderAttack, "Attack", "Attack",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {0.1f, 200.0f, 5.0f, 0.0f, 1.0f}, nullptr, 0, "attack", "ms", "Expander", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamExpanderRelease, "Release", "Release",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {1.0f, 2000.0f, 100.0f, 0.0f, 1.0f}, nullptr, 0, "release", "ms", "Expander", 2),
        ParameterSmoothingMode::Linear);
}

void ExpanderVST3Processor::applyParametersToDSP () noexcept
{
    const float newThreshold = parameters_.getCurrentReal (kParamExpanderThreshold);
    const float newRatio = parameters_.getCurrentReal (kParamExpanderRatio);
    const float newAttack = parameters_.getCurrentReal (kParamExpanderAttack);
    const float newRelease = parameters_.getCurrentReal (kParamExpanderRelease);

    if (dspPrepared_ && newThreshold == thresholdDB_ && newRatio == ratio_ &&
        newAttack == attackMs_ && newRelease == releaseMs_)
    {
        return;
    }

    thresholdDB_ = newThreshold;
    ratio_ = newRatio;
    attackMs_ = newAttack;
    releaseMs_ = newRelease;

    for (auto& expander : expanders_)
        expander.prepare (static_cast<float> (sampleRate_), attackMs_, releaseMs_, thresholdDB_, ratio_);

    dspPrepared_ = true;
}

} // namespace CV
