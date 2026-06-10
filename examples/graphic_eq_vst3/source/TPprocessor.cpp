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
constexpr float kGainDefault = 0.0f;
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;
constexpr const char* kGainNames[10] {"31 Hz", "63 Hz", "125 Hz", "250 Hz", "500 Hz",
                                      "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};
}

//------------------------------------------------------------------------
GraphicEQVST3Processor::GraphicEQVST3Processor ()
{
    setControllerClass (kGraphicEQVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

//------------------------------------------------------------------------
GraphicEQVST3Processor::~GraphicEQVST3Processor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& eq : eqs_)
            eq.reset ();
    }
    return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : sampleRate_;
    for (auto& eq : eqs_)
        eq.prepare (static_cast<float> (sampleRate_));

    cvdsp::manager::ParameterSmoothingConfig<float> smoothing {};
    smoothing.sampleRate = static_cast<float> (sampleRate_);
    smoothing.rampTimeSeconds = 0.010f;
    (void)parameters_.prepare (static_cast<float> (sampleRate_), static_cast<std::size_t> (newSetup.maxSamplesPerBlock), smoothing);
    applyParametersToDSP ();

    return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::process (Vst::ProcessData& data)
{
    parameters_.beginBlock (static_cast<std::size_t> (std::max<int32> (data.numSamples, 0)));
    cvdsp::adapters::vst3::VST3ParameterAdapter::adaptParameterChanges (data.inputParameterChanges, parameters_);
    parameters_.processBlockParameters (static_cast<std::size_t> (std::max<int32> (data.numSamples, 0)));
    applyParametersToDSP ();

    if (data.numSamples <= 0)
        return kResultOk;

    const int32 minBus = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < minBus; ++bus)
    {
        const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (eqs_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            if (output != input)
                std::memcpy (output, input, static_cast<std::size_t> (data.numSamples) * sizeof (Vst::Sample32));
            eqs_[static_cast<std::size_t> (channel)].processBlock (output, static_cast<std::size_t> (data.numSamples));
        }

        for (int32 channel = processChan; channel < minChan; ++channel)
        {
            if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
            {
                std::memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel],
                             static_cast<std::size_t> (data.numSamples) * sizeof (Vst::Sample32));
            }
        }
        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 band = 0; band < static_cast<int32> (kGraphicEQBandCount); ++band)
    {
        float gain = kGainDefault;
        if (!streamer.readFloat (gain))
            return kResultFalse;
        (void)parameters_.setImmediateReal (graphicEQGainParamID (band), gain);
    }
    applyParametersToDSP ();
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GraphicEQVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 band = 0; band < static_cast<int32> (kGraphicEQBandCount); ++band)
    {
        const float gain = parameters_.getCurrentReal (graphicEQGainParamID (band));
        if (!streamer.writeFloat (gain))
            return kResultFalse;
    }
    return kResultOk;
}

void GraphicEQVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    for (cvdsp::u32 band = 0; band < kGraphicEQBandCount; ++band)
    {
        (void)parameters_.registerParameter (ParameterDescriptor<float> (graphicEQGainParamID (band),
            "Gain", kGainNames[band], ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
            {-24.0f, 24.0f, kGainDefault, 0.0f, 1.0f}, nullptr, 0, "gain", "dB", "Graphic EQ", 2),
            ParameterSmoothingMode::Linear);
    }
}

void GraphicEQVST3Processor::applyParametersToDSP () noexcept
{
    for (std::size_t band = 0; band < static_cast<std::size_t> (kGraphicEQBandCount); ++band)
    {
        gainsDB_[band] = parameters_.getCurrentReal (graphicEQGainParamID (static_cast<cvdsp::u32> (band)));
        for (auto& eq : eqs_)
            (void)eq.setBandGainDB (band, gainsDB_[band]);
    }
}

//------------------------------------------------------------------------
} // namespace CV
