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

OscillatorVST3Processor::OscillatorVST3Processor ()
{
    setControllerClass (kOscillatorVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

OscillatorVST3Processor::~OscillatorVST3Processor () {}

tresult PLUGIN_API OscillatorVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API OscillatorVST3Processor::terminate () { return AudioEffect::terminate (); }

tresult PLUGIN_API OscillatorVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& osc : oscillators_)
            osc.reset ();
        lastAppliedPhaseDegrees_ = -1.0f;
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API OscillatorVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    for (auto& osc : oscillators_)
        osc.prepare (static_cast<float> (sampleRate_), frequencyHz_, cvdsp::modulation::OscillatorWaveform::Sine);
    lastAppliedPhaseDegrees_ = -1.0f;
    applyParametersToDSP ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API OscillatorVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API OscillatorVST3Processor::process (Vst::ProcessData& data)
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

    for (int32 bus = 0; bus < data.numOutputs; ++bus)
    {
        const int32 processChan = std::min<int32> (data.outputs[bus].numChannels, static_cast<int32> (oscillators_.size ()));
        for (int32 channel = 0; channel < processChan; ++channel)
        {
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            auto& osc = oscillators_[static_cast<std::size_t> (channel)];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = osc.process ();
        }
        for (int32 channel = processChan; channel < data.outputs[bus].numChannels; ++channel)
            std::fill_n (data.outputs[bus].channelBuffers32[channel], sampleCount, 0.0f);
        data.outputs[bus].silenceFlags = 0;
    }
    return kResultOk;
}

tresult PLUGIN_API OscillatorVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float waveform = 0.0f, frequency = 440.0f, phase = 0.0f;
    if (!streamer.readFloat (waveform) || !streamer.readFloat (frequency) || !streamer.readFloat (phase))
        return kResultFalse;
    (void)parameters_.setImmediateReal (kParamOscillatorWaveform, waveform);
    (void)parameters_.setImmediateReal (kParamOscillatorFrequency, frequency);
    (void)parameters_.setImmediateReal (kParamOscillatorPhase, phase);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API OscillatorVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (waveformIndex_) || !streamer.writeFloat (frequencyHz_) || !streamer.writeFloat (phaseDegrees_))
        return kResultFalse;
    return kResultOk;
}

void OscillatorVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamOscillatorWaveform, "Waveform", "Waveform", ParameterUnit::None, ParameterScale::Enum, kParamFlags, {0.0f, 3.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "waveform", "", "Oscillator", 2), ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamOscillatorFrequency, "Frequency", "Frequency", ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags, {20.0f, 20000.0f, 440.0f, 0.0f, 1.0f}, nullptr, 0, "frequency", "Hz", "Oscillator", 2), ParameterSmoothingMode::Linear);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamOscillatorPhase, "Phase", "Phase", ParameterUnit::Degrees, ParameterScale::Linear, kParamFlags, {0.0f, 360.0f, 0.0f, 0.0f, 1.0f}, nullptr, 0, "phase", "deg", "Oscillator", 2), ParameterSmoothingMode::Linear);
}

void OscillatorVST3Processor::applyParametersToDSP () noexcept
{
    waveformIndex_ = parameters_.getCurrentReal (kParamOscillatorWaveform);
    frequencyHz_ = parameters_.getCurrentReal (kParamOscillatorFrequency);
    phaseDegrees_ = parameters_.getCurrentReal (kParamOscillatorPhase);
    const int wave = std::clamp (static_cast<int> (waveformIndex_ + 0.5f), 0, 3);
    const auto waveform = static_cast<cvdsp::modulation::OscillatorWaveform> (wave);
    for (auto& osc : oscillators_)
    {
        osc.setWaveform (waveform);
        osc.setFrequency (frequencyHz_);
        if (phaseDegrees_ != lastAppliedPhaseDegrees_)
            osc.setPhase (phaseDegrees_ / 360.0f);
    }
    lastAppliedPhaseDegrees_ = phaseDegrees_;
}

} // namespace CV
