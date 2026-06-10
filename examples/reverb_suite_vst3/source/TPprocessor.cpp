//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cstring>
#include <cmath>

using namespace Steinberg;

namespace CV {
namespace {
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;

constexpr cvdsp::manager::ParameterFlags kEnumParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent |
    cvdsp::manager::ParameterFlag::Enum | cvdsp::manager::ParameterFlag::Discrete;

float clamp01 (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

float normalize (float plain, float minPlain, float maxPlain) noexcept
{
    return (plain - minPlain) / (maxPlain - minPlain);
}

float denormalize (float normalized, float minPlain, float maxPlain) noexcept
{
    return minPlain + (clamp01 (normalized) * (maxPlain - minPlain));
}
}

ReverbSuiteVST3Processor::ReverbSuiteVST3Processor ()
{
    setControllerClass (kReverbSuiteVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

ReverbSuiteVST3Processor::~ReverbSuiteVST3Processor ()
{}

tresult PLUGIN_API ReverbSuiteVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API ReverbSuiteVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API ReverbSuiteVST3Processor::setActive (TBool state)
{
    if (state)
        resetDSP ();

    return AudioEffect::setActive (state);
}

tresult PLUGIN_API ReverbSuiteVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
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

tresult PLUGIN_API ReverbSuiteVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API ReverbSuiteVST3Processor::process (Vst::ProcessData& data)
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

            processSelectedReverb (view);
        }

        data.outputs[bus].silenceFlags = 0;
    }

    return kResultOk;
}

tresult PLUGIN_API ReverbSuiteVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float algorithm = 0.0f;
    float mix = 0.25f;
    float decay = 0.50f;
    float tone = 0.60f;
    float preDelay = 20.0f;
    float widthDwell = 0.60f;

    if (!streamer.readFloat (algorithm) || !streamer.readFloat (mix) || !streamer.readFloat (decay) ||
        !streamer.readFloat (tone) || !streamer.readFloat (preDelay) || !streamer.readFloat (widthDwell))
        return kResultFalse;

    (void)parameters_.setImmediateReal (kParamAlgorithm, algorithm);
    (void)parameters_.setImmediateReal (kParamMix, mix);
    (void)parameters_.setImmediateReal (kParamDecay, decay);
    (void)parameters_.setImmediateReal (kParamTone, tone);
    (void)parameters_.setImmediateReal (kParamPreDelay, preDelay);
    (void)parameters_.setImmediateReal (kParamWidthDwell, widthDwell);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API ReverbSuiteVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (static_cast<float> (algorithm_)) || !streamer.writeFloat (mix_) ||
        !streamer.writeFloat (decay_) || !streamer.writeFloat (tone_) || !streamer.writeFloat (preDelayMs_) ||
        !streamer.writeFloat (widthDwell_))
        return kResultFalse;

    return kResultOk;
}

void ReverbSuiteVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;

    static constexpr ParameterEnumEntry algorithmEntries[] {
        {0u, "Room"},
        {1u, "Hall"},
        {2u, "Plate"},
        {3u, "Spring"},
        {4u, "Twin"},
        {5u, "Deluxe"},
        {6u, "Super"}
    };

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamAlgorithm, "Model", "Reverb Model", ParameterUnit::Index, ParameterScale::Enum, kEnumParamFlags,
        ParameterRange<float> {0.0f, 6.0f, 0.0f, 1.0f}, algorithmEntries, 7, "algorithm", nullptr, "Reverb Suite", 0));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamMix, "Mix", "Wet/Dry Mix", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.25f}, nullptr, 0, "mix", "%", "Reverb Suite", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamDecay, "Decay", "Decay / RT60 / Amount", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.50f}, nullptr, 0, "decay", "%", "Reverb Suite", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamTone, "Tone", "Tone / Damping", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.60f}, nullptr, 0, "tone", "%", "Reverb Suite", 2));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamPreDelay, "PreDelay", "Pre-Delay", ParameterUnit::Milliseconds, ParameterScale::Linear, kParamFlags,
        ParameterRange<float> {0.0f, 250.0f, 20.0f}, nullptr, 0, "predelay", "ms", "Reverb Suite", 1));

    (void)parameters_.registerParameter (ParameterDescriptor<float> (
        kParamWidthDwell, "Width/Dwell", "Width / Dwell", ParameterUnit::Percent, ParameterScale::Percentage, kParamFlags,
        ParameterRange<float> {0.0f, 1.0f, 0.60f}, nullptr, 0, "width_dwell", "%", "Reverb Suite", 2));
}

void ReverbSuiteVST3Processor::prepareDSP () noexcept
{
    room_.prepare (dspContext_);
    hall_.prepare (dspContext_);
    plate_.prepare (dspContext_);
    spring_.prepare (dspContext_);
    twin_.prepare (dspContext_);
    deluxe_.prepare (dspContext_);
    super_.prepare (dspContext_);
}

void ReverbSuiteVST3Processor::resetDSP () noexcept
{
    room_.reset ();
    hall_.reset ();
    plate_.reset ();
    spring_.reset ();
    twin_.reset ();
    deluxe_.reset ();
    super_.reset ();
}

void ReverbSuiteVST3Processor::applyParametersToDSP () noexcept
{
    algorithm_ = static_cast<int32> (std::lround (parameters_.getCurrentReal (kParamAlgorithm)));
    algorithm_ = std::clamp<int32> (algorithm_, 0, static_cast<int32> (Algorithm::Count) - 1);

    mix_ = clamp01 (parameters_.getCurrentReal (kParamMix));
    decay_ = clamp01 (parameters_.getCurrentReal (kParamDecay));
    tone_ = clamp01 (parameters_.getCurrentReal (kParamTone));
    preDelayMs_ = std::clamp (parameters_.getCurrentReal (kParamPreDelay), 0.0f, 250.0f);
    widthDwell_ = clamp01 (parameters_.getCurrentReal (kParamWidthDwell));

    const float dry = 1.0f - mix_;
    const float damping = 1.0f - tone_;

    room_.setWet (mix_);
    room_.setDry (dry);
    room_.setDecayTime (denormalize (decay_, 0.05f, 1.20f));
    room_.setDamping (damping);
    room_.setRoomSize (widthDwell_);

    hall_.setWet (mix_);
    hall_.setDry (dry);
    hall_.setRT60 (denormalize (decay_, 0.30f, 12.0f));
    hall_.setDamping (damping);
    hall_.setPreDelay (preDelayMs_);
    hall_.setWidth (widthDwell_);
    hall_.setHallSize (std::max (decay_, widthDwell_));

    plate_.setWet (mix_);
    plate_.setDry (dry);
    plate_.setDecay (decay_);
    plate_.setDamping (damping);
    plate_.setBrightness (tone_);
    plate_.setPreDelay (std::min (preDelayMs_, 120.0f));

    spring_.setMix (mix_);
    spring_.setSpringLength (decay_);
    spring_.setTone (tone_);
    spring_.setTension (widthDwell_);

    twin_.setMix (mix_);
    twin_.setReverbAmount (decay_);
    twin_.setDwell (widthDwell_);
    twin_.setTone (tone_);

    deluxe_.setMix (mix_);
    deluxe_.setReverbAmount (decay_);
    deluxe_.setDwell (widthDwell_);
    deluxe_.setTone (tone_);

    super_.setMix (mix_);
    super_.setReverbAmount (decay_);
    super_.setDwell (widthDwell_);
    super_.setTone (tone_);
    super_.setWidth (widthDwell_);
}

void ReverbSuiteVST3Processor::processSelectedReverb (cvdsp::AudioBufferView<float>& buffer) noexcept
{
    switch (static_cast<Algorithm> (algorithm_))
    {
        case Algorithm::Room:
            room_.processBlock (buffer);
            break;
        case Algorithm::Hall:
            hall_.processBlock (buffer);
            break;
        case Algorithm::Plate:
            plate_.processBlock (buffer);
            break;
        case Algorithm::Spring:
            spring_.processBlock (buffer);
            break;
        case Algorithm::Twin:
            twin_.processBlock (buffer);
            break;
        case Algorithm::Deluxe:
            deluxe_.processBlock (buffer);
            break;
        case Algorithm::Super:
            super_.processBlock (buffer);
            break;
        case Algorithm::Count:
        default:
            room_.processBlock (buffer);
            break;
    }
}

} // namespace CV
