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

NoiseGateVST3Processor::NoiseGateVST3Processor ()
{
    setControllerClass (kNoiseGateVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

NoiseGateVST3Processor::~NoiseGateVST3Processor ()
{}

tresult PLUGIN_API NoiseGateVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API NoiseGateVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API NoiseGateVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& gate : gates_)
            gate.reset ();
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API NoiseGateVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& gate : gates_)
        gate.prepare (static_cast<float> (sampleRate_));
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API NoiseGateVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API NoiseGateVST3Processor::process (Vst::ProcessData& data)
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
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (gates_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& gate = gates_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = gate.process (input[sample]);
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

tresult PLUGIN_API NoiseGateVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float open = -40.0f;
    float close = -45.0f;
    float attack = 1.0f;
    float hold = 50.0f;
    float release = 30.0f;
    if (!streamer.readFloat (open) || !streamer.readFloat (close) || !streamer.readFloat (attack) ||
        !streamer.readFloat (hold) || !streamer.readFloat (release))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamNoiseGateThresholdOpen, open);
    (void)parameters_.setImmediateReal (kParamNoiseGateThresholdClose, close);
    (void)parameters_.setImmediateReal (kParamNoiseGateAttack, attack);
    (void)parameters_.setImmediateReal (kParamNoiseGateHold, hold);
    (void)parameters_.setImmediateReal (kParamNoiseGateRelease, release);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API NoiseGateVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (thresholdOpenDB_) || !streamer.writeFloat (thresholdCloseDB_) ||
        !streamer.writeFloat (attackMs_) || !streamer.writeFloat (holdMs_) || !streamer.writeFloat (releaseMs_))
        return kResultFalse;
    return kResultOk;
}

void NoiseGateVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamNoiseGateThresholdOpen, "Open", "Threshold Open",
        ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
        {-80.0f, 0.0f, -40.0f, 0.0f, 1.0f}, nullptr, 0, "threshold_open", "dBFS", "Noise Gate", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamNoiseGateThresholdClose, "Close", "Threshold Close",
        ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
        {-90.0f, 0.0f, -45.0f, 0.0f, 1.0f}, nullptr, 0, "threshold_close", "dBFS", "Noise Gate", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamNoiseGateAttack, "Attack", "Attack",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {0.1f, 100.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "attack", "ms", "Noise Gate", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamNoiseGateHold, "Hold", "Hold",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {0.0f, 500.0f, 50.0f, 0.0f, 1.0f}, nullptr, 0, "hold", "ms", "Noise Gate", 2),
        ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamNoiseGateRelease, "Release", "Release",
        ParameterUnit::Milliseconds, ParameterScale::Logarithmic, kParamFlags,
        {1.0f, 1000.0f, 30.0f, 0.0f, 1.0f}, nullptr, 0, "release", "ms", "Noise Gate", 2),
        ParameterSmoothingMode::Linear);
}

void NoiseGateVST3Processor::applyParametersToDSP () noexcept
{
    thresholdOpenDB_ = parameters_.getCurrentReal (kParamNoiseGateThresholdOpen);
    thresholdCloseDB_ = parameters_.getCurrentReal (kParamNoiseGateThresholdClose);
    attackMs_ = parameters_.getCurrentReal (kParamNoiseGateAttack);
    holdMs_ = parameters_.getCurrentReal (kParamNoiseGateHold);
    releaseMs_ = parameters_.getCurrentReal (kParamNoiseGateRelease);

    if (thresholdCloseDB_ > thresholdOpenDB_)
        thresholdCloseDB_ = thresholdOpenDB_;

    for (auto& gate : gates_)
    {
        gate.setThresholdOpenDB (thresholdOpenDB_);
        gate.setThresholdCloseDB (thresholdCloseDB_);
        gate.setAttackMs (attackMs_);
        gate.setHoldMs (holdMs_);
        gate.setReleaseMs (releaseMs_);
    }
}

} // namespace CV
