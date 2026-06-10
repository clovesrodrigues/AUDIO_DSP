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
constexpr float kFrequencyDefaults[5] {80.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f};
constexpr float kQDefault = 0.707f;
constexpr float kGainDefault = 0.0f;
constexpr int32 kTypeDefaults[5] {kParametricEQLowShelf, kParametricEQPeaking, kParametricEQPeaking,
                                  kParametricEQPeaking, kParametricEQHighShelf};
constexpr cvdsp::manager::ParameterEnumEntry kTypeEntries[] {
    {kParametricEQLowPass, "LowPass"}, {kParametricEQHighPass, "HighPass"},
    {kParametricEQPeaking, "Peaking"}, {kParametricEQLowShelf, "LowShelf"},
    {kParametricEQHighShelf, "HighShelf"}, {kParametricEQNotch, "Notch"}};
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;
}

//------------------------------------------------------------------------
ParametricEQVST3Processor::ParametricEQVST3Processor ()
{
    setControllerClass (kParametricEQVST3ControllerUID);
    setDefaults ();
    registerParameters ();
    applyParametersToDSP ();
}

//------------------------------------------------------------------------
ParametricEQVST3Processor::~ParametricEQVST3Processor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& eq : eqs_)
            eq.reset ();
    }
    return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
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
tresult PLUGIN_API ParametricEQVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Processor::process (Vst::ProcessData& data)
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
tresult PLUGIN_API ParametricEQVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 band = 0; band < static_cast<int32> (kParametricEQBandCount); ++band)
    {
        int32 type = kTypeDefaults[band];
        float frequency = kFrequencyDefaults[band];
        float q = kQDefault;
        float gain = kGainDefault;

        if (!streamer.readInt32 (type) || !streamer.readFloat (frequency) ||
            !streamer.readFloat (q) || !streamer.readFloat (gain))
        {
            return kResultFalse;
        }

        (void)parameters_.setImmediateNormalized (parametricEQParamID (band, kParametricEQTypeOffset), static_cast<float> (typeIndexToNormalized (type)));
        (void)parameters_.setImmediateReal (parametricEQParamID (band, kParametricEQFrequencyOffset), frequency);
        (void)parameters_.setImmediateReal (parametricEQParamID (band, kParametricEQQOffset), q);
        (void)parameters_.setImmediateReal (parametricEQParamID (band, kParametricEQGainOffset), gain);
    }
    applyParametersToDSP ();
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ParametricEQVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (int32 band = 0; band < static_cast<int32> (kParametricEQBandCount); ++band)
    {
        const int32 type = normalizedToTypeIndex (parameters_.getTargetNormalized (parametricEQParamID (band, kParametricEQTypeOffset)));
        const float frequency = parameters_.getCurrentReal (parametricEQParamID (band, kParametricEQFrequencyOffset));
        const float q = parameters_.getCurrentReal (parametricEQParamID (band, kParametricEQQOffset));
        const float gain = parameters_.getCurrentReal (parametricEQParamID (band, kParametricEQGainOffset));

        if (!streamer.writeInt32 (type) || !streamer.writeFloat (frequency) ||
            !streamer.writeFloat (q) || !streamer.writeFloat (gain))
        {
            return kResultFalse;
        }
    }
    return kResultOk;
}

void ParametricEQVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    for (cvdsp::u32 band = 0; band < kParametricEQBandCount; ++band)
    {
        const cvdsp::u32 bandNumber = band + 1;
        (void)bandNumber;
        (void)parameters_.registerParameter (ParameterDescriptor<float> (parametricEQParamID (band, kParametricEQTypeOffset),
            "Type", "Band Type", ParameterUnit::Index, ParameterScale::Enum, kParamFlags,
            {0.0f, static_cast<float> (kParametricEQFilterTypeCount - 1), static_cast<float> (kTypeDefaults[band]), 1.0f, 1.0f},
            kTypeEntries, kParametricEQFilterTypeCount, "type", "", "Parametric EQ", 0), ParameterSmoothingMode::None);
        (void)parameters_.registerParameter (ParameterDescriptor<float> (parametricEQParamID (band, kParametricEQFrequencyOffset),
            "Freq", "Frequency", ParameterUnit::Hertz, ParameterScale::Logarithmic, kParamFlags,
            {20.0f, 20000.0f, kFrequencyDefaults[band], 0.0f, 1.0f}, nullptr, 0, "frequency", "Hz", "Parametric EQ", 2), ParameterSmoothingMode::Linear);
        (void)parameters_.registerParameter (ParameterDescriptor<float> (parametricEQParamID (band, kParametricEQQOffset),
            "Q", "Q", ParameterUnit::None, ParameterScale::Logarithmic, kParamFlags,
            {0.10f, 10.0f, kQDefault, 0.0f, 1.0f}, nullptr, 0, "q", "", "Parametric EQ", 2), ParameterSmoothingMode::Linear);
        (void)parameters_.registerParameter (ParameterDescriptor<float> (parametricEQParamID (band, kParametricEQGainOffset),
            "Gain", "Gain", ParameterUnit::Decibels, ParameterScale::Decibel, kParamFlags,
            {-24.0f, 24.0f, kGainDefault, 0.0f, 1.0f}, nullptr, 0, "gain", "dB", "Parametric EQ", 2), ParameterSmoothingMode::Linear);
    }
}

void ParametricEQVST3Processor::applyParametersToDSP () noexcept
{
    for (std::size_t band = 0; band < static_cast<std::size_t> (kParametricEQBandCount); ++band)
    {
        type_[band] = static_cast<float> (normalizedToTypeIndex (parameters_.getTargetNormalized (parametricEQParamID (band, kParametricEQTypeOffset))));
        frequencyHz_[band] = parameters_.getCurrentReal (parametricEQParamID (static_cast<cvdsp::u32> (band), kParametricEQFrequencyOffset));
        q_[band] = parameters_.getCurrentReal (parametricEQParamID (static_cast<cvdsp::u32> (band), kParametricEQQOffset));
        gainDB_[band] = parameters_.getCurrentReal (parametricEQParamID (static_cast<cvdsp::u32> (band), kParametricEQGainOffset));

        for (auto& eq : eqs_)
        {
            (void)eq.setBand (band, toBiquadType (static_cast<int32> (type_[band])), frequencyHz_[band], q_[band], gainDB_[band], true);
        }
    }
}

void ParametricEQVST3Processor::setDefaults () noexcept
{
    for (std::size_t band = 0; band < static_cast<std::size_t> (kParametricEQBandCount); ++band)
    {
        type_[band] = static_cast<float> (kTypeDefaults[band]);
        frequencyHz_[band] = kFrequencyDefaults[band];
        q_[band] = kQDefault;
        gainDB_[band] = kGainDefault;
    }
}

cvdsp::filters::BiquadType ParametricEQVST3Processor::toBiquadType (int32 typeIndex) noexcept
{
    switch (typeIndex)
    {
        case kParametricEQLowPass: return cvdsp::filters::BiquadType::LowPass;
        case kParametricEQHighPass: return cvdsp::filters::BiquadType::HighPass;
        case kParametricEQLowShelf: return cvdsp::filters::BiquadType::LowShelf;
        case kParametricEQHighShelf: return cvdsp::filters::BiquadType::HighShelf;
        case kParametricEQNotch: return cvdsp::filters::BiquadType::Notch;
        case kParametricEQPeaking:
        default: return cvdsp::filters::BiquadType::PeakingEQ;
    }
}

int32 ParametricEQVST3Processor::normalizedToTypeIndex (float normalized) noexcept
{
    const float scaled = std::clamp (normalized, 0.0f, 1.0f) * static_cast<float> (kParametricEQFilterTypeCount - 1);
    return std::clamp<int32> (static_cast<int32> (scaled + 0.5f), 0, kParametricEQFilterTypeCount - 1);
}

Vst::ParamValue ParametricEQVST3Processor::typeIndexToNormalized (int32 typeIndex) noexcept
{
    return static_cast<Vst::ParamValue> (std::clamp<int32> (typeIndex, 0, kParametricEQFilterTypeCount - 1)) /
           static_cast<Vst::ParamValue> (kParametricEQFilterTypeCount - 1);
}

Vst::ParamValue ParametricEQVST3Processor::plainToNormalized (float plain, float minPlain, float maxPlain) noexcept
{
    return (static_cast<Vst::ParamValue> (plain) - minPlain) / (maxPlain - minPlain);
}

//------------------------------------------------------------------------
} // namespace CV
