//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace CV {
namespace {
constexpr float kThresholdMin = -60.0f;
constexpr float kThresholdMax = 0.0f;
constexpr float kRatioMin = 1.0f;
constexpr float kRatioMax = 20.0f;
constexpr float kAttackMin = 0.1f;
constexpr float kAttackMax = 200.0f;
constexpr float kReleaseMin = 10.0f;
constexpr float kReleaseMax = 1000.0f;
constexpr float kKneeMin = 0.0f;
constexpr float kKneeMax = 24.0f;
constexpr float kMakeupMin = 0.0f;
constexpr float kMakeupMax = 24.0f;
}

//------------------------------------------------------------------------
// CompressorVST3Processor
//------------------------------------------------------------------------
CompressorVST3Processor::CompressorVST3Processor ()
{
    setControllerClass (kCompressorVST3ControllerUID);
    applyParameters ();
}

//------------------------------------------------------------------------
CompressorVST3Processor::~CompressorVST3Processor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::setActive (TBool state)
{
    if (state)
    {
        for (auto& compressor : compressors_)
            compressor.reset ();
    }
    return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::process (Vst::ProcessData& data)
{
    if (data.inputParameterChanges)
    {
        const int32 numParamsChanged = data.inputParameterChanges->getParameterCount ();
        for (int32 index = 0; index < numParamsChanged; ++index)
        {
            if (auto* paramQueue = data.inputParameterChanges->getParameterData (index))
            {
                Vst::ParamValue value = 0.0;
                int32 sampleOffset = 0;
                const int32 pointCount = paramQueue->getPointCount ();
                if (pointCount <= 0 || paramQueue->getPoint (pointCount - 1, sampleOffset, value) != kResultTrue)
                    continue;

                switch (paramQueue->getParameterId ())
                {
                    case kParamThreshold:
                        thresholdDB_ = normalizedToPlain (value, kThresholdMin, kThresholdMax);
                        break;
                    case kParamRatio:
                        ratio_ = normalizedToPlain (value, kRatioMin, kRatioMax);
                        break;
                    case kParamAttack:
                        attackMs_ = normalizedToPlain (value, kAttackMin, kAttackMax);
                        break;
                    case kParamRelease:
                        releaseMs_ = normalizedToPlain (value, kReleaseMin, kReleaseMax);
                        break;
                    case kParamKnee:
                        kneeDB_ = normalizedToPlain (value, kKneeMin, kKneeMax);
                        break;
                    case kParamMakeup:
                        makeupDB_ = normalizedToPlain (value, kMakeupMin, kMakeupMax);
                        break;
                    case kParamDetector:
                        detectorIndex_ = normalizedToDetectorIndex (value);
                        break;
                    default:
                        break;
                }
            }
        }
        applyParameters ();
    }

    if (data.numSamples <= 0)
        return kResultOk;

    const int32 minBus = std::min (data.numInputs, data.numOutputs);
    for (int32 bus = 0; bus < minBus; ++bus)
    {
        const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        const int32 processChan = std::min<int32> (minChan, static_cast<int32> (compressors_.size ()));

        for (int32 channel = 0; channel < processChan; ++channel)
        {
            const Vst::Sample32* input = data.inputs[bus].channelBuffers32[channel];
            Vst::Sample32* output = data.outputs[bus].channelBuffers32[channel];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
                output[sample] = compressors_[static_cast<std::size_t> (channel)].process (input[sample]);
        }

        for (int32 channel = processChan; channel < minChan; ++channel)
        {
            if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
            {
                std::memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel],
                             data.numSamples * sizeof (Vst::Sample32));
            }
        }

        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;

        for (int32 channel = minChan; channel < data.outputs[bus].numChannels; ++channel)
        {
            std::memset (data.outputs[bus].channelBuffers32[channel], 0,
                         data.numSamples * sizeof (Vst::Sample32));
            data.outputs[bus].silenceFlags |= (static_cast<uint64> (1) << channel);
        }
    }

    for (int32 bus = minBus; bus < data.numOutputs; ++bus)
    {
        for (int32 channel = 0; channel < data.outputs[bus].numChannels; ++channel)
        {
            std::memset (data.outputs[bus].channelBuffers32[channel], 0,
                         data.numSamples * sizeof (Vst::Sample32));
        }
        data.outputs[bus].silenceFlags = (static_cast<uint64> (1) << data.outputs[bus].numChannels) - 1;
    }

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : sampleRate_;
    for (auto& compressor : compressors_)
        compressor.prepare (static_cast<float> (sampleRate_));
    applyParameters ();

    return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;

    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.readFloat (thresholdDB_) ||
        !streamer.readFloat (ratio_) ||
        !streamer.readFloat (attackMs_) ||
        !streamer.readFloat (releaseMs_) ||
        !streamer.readFloat (kneeDB_) ||
        !streamer.readFloat (makeupDB_))
    {
        return kResultFalse;
    }

    int32 detector = 0;
    if (!streamer.readInt32 (detector))
        return kResultFalse;

    detectorIndex_ = std::clamp<int32> (detector, 0, 1);
    applyParameters ();

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API CompressorVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (thresholdDB_) ||
        !streamer.writeFloat (ratio_) ||
        !streamer.writeFloat (attackMs_) ||
        !streamer.writeFloat (releaseMs_) ||
        !streamer.writeFloat (kneeDB_) ||
        !streamer.writeFloat (makeupDB_) ||
        !streamer.writeInt32 (detectorIndex_))
    {
        return kResultFalse;
    }

    return kResultOk;
}

//------------------------------------------------------------------------
void CompressorVST3Processor::applyParameters () noexcept
{
    thresholdDB_ = std::clamp (thresholdDB_, kThresholdMin, kThresholdMax);
    ratio_ = std::clamp (ratio_, kRatioMin, kRatioMax);
    attackMs_ = std::clamp (attackMs_, kAttackMin, kAttackMax);
    releaseMs_ = std::clamp (releaseMs_, kReleaseMin, kReleaseMax);
    kneeDB_ = std::clamp (kneeDB_, kKneeMin, kKneeMax);
    makeupDB_ = std::clamp (makeupDB_, kMakeupMin, kMakeupMax);
    detectorIndex_ = std::clamp<int32> (detectorIndex_, 0, 1);

    for (auto& compressor : compressors_)
    {
        compressor.setThresholdDB (thresholdDB_);
        compressor.setRatio (ratio_);
        compressor.setAttackMs (attackMs_);
        compressor.setReleaseMs (releaseMs_);
        compressor.setKneeDB (kneeDB_);
        compressor.setMakeupGainDB (makeupDB_);
    }
    setDetectorFromIndex (detectorIndex_);
}

//------------------------------------------------------------------------
void CompressorVST3Processor::setDetectorFromIndex (int32 detectorIndex) noexcept
{
    const auto mode = detectorIndex == 1 ? cvdsp::dynamics::EnvelopeMode::RMS
                                        : cvdsp::dynamics::EnvelopeMode::Peak;
    for (auto& compressor : compressors_)
        compressor.setDetectionMode (mode);
}

//------------------------------------------------------------------------
float CompressorVST3Processor::normalizedToPlain (Vst::ParamValue normalized, float minPlain, float maxPlain) noexcept
{
    const auto clamped = std::clamp (normalized, 0.0, 1.0);
    return static_cast<float> (minPlain + (maxPlain - minPlain) * clamped);
}

//------------------------------------------------------------------------
Vst::ParamValue CompressorVST3Processor::plainToNormalized (float plain, float minPlain, float maxPlain) noexcept
{
    if (maxPlain <= minPlain)
        return 0.0;

    const auto normalized = (plain - minPlain) / (maxPlain - minPlain);
    return std::clamp<Vst::ParamValue> (normalized, 0.0, 1.0);
}

//------------------------------------------------------------------------
int32 CompressorVST3Processor::normalizedToDetectorIndex (Vst::ParamValue normalized) noexcept
{
    return normalized >= 0.5 ? 1 : 0;
}

//------------------------------------------------------------------------
Vst::ParamValue CompressorVST3Processor::detectorIndexToNormalized (int32 detectorIndex) noexcept
{
    return detectorIndex == 1 ? 1.0 : 0.0;
}

//------------------------------------------------------------------------
} // namespace CV
