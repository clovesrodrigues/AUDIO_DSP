//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace CV {
namespace {
constexpr cvdsp::manager::ParameterFlags kParamFlags =
    cvdsp::manager::ParameterFlag::Automatable | cvdsp::manager::ParameterFlag::Persistent;

cvdsp::spectral::WindowType windowTypeFromIndex (int index) noexcept
{
    switch (std::clamp (index, 0, 5))
    {
        case 0: return cvdsp::spectral::WindowType::Rectangular;
        case 1: return cvdsp::spectral::WindowType::Hann;
        case 2: return cvdsp::spectral::WindowType::Hamming;
        case 3: return cvdsp::spectral::WindowType::Blackman;
        case 4: return cvdsp::spectral::WindowType::BlackmanHarris;
        default: return cvdsp::spectral::WindowType::Kaiser;
    }
}

std::size_t fftSizeFromIndex (int index) noexcept
{
    switch (std::clamp (index, 0, 2))
    {
        case 0: return 512;
        case 1: return 1024;
        default: return 2048;
    }
}
} // namespace

SpectrumAnalyzerVST3Processor::SpectrumAnalyzerVST3Processor ()
{
    setControllerClass (kSpectrumAnalyzerVST3ControllerUID);
    registerParameters ();
    applyParametersToDSP ();
}

SpectrumAnalyzerVST3Processor::~SpectrumAnalyzerVST3Processor ()
{}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;
    addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);
    return result;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    sampleRate_ = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;
    configureAnalyzer ();
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::process (Vst::ProcessData& data)
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
        const int32 minChannels = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
        if (minChannels >= 2)
        {
            processStereoPassThrough (data.inputs[bus].channelBuffers32[0],
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
            const auto* input = data.inputs[bus].channelBuffers32[0];
            auto* output = data.outputs[bus].channelBuffers32[0];
            for (int32 sample = 0; sample < data.numSamples; ++sample)
            {
                processAnalysisSample (input[sample]);
                output[sample] = input[sample];
            }
        }
        data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;
    }
    return kResultOk;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float window = windowIndex_;
    float fftSize = fftSizeIndex_;
    if (!streamer.readFloat (window) || !streamer.readFloat (fftSize))
        return kResultFalse;
    (void)parameters_.setImmediateReal (kParamSpectrumAnalyzerVST3Window, window);
    (void)parameters_.setImmediateReal (kParamSpectrumAnalyzerVST3FFTSize, fftSize);
    applyParametersToDSP ();
    return kResultOk;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Processor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    if (!streamer.writeFloat (windowIndex_) || !streamer.writeFloat (fftSizeIndex_))
        return kResultFalse;
    return kResultOk;
}

void SpectrumAnalyzerVST3Processor::registerParameters () noexcept
{
    using namespace cvdsp::manager;
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamSpectrumAnalyzerVST3Window, "Windowing", "Windowing",
        ParameterUnit::None, ParameterScale::Enum, kParamFlags,
        {0.0f, 5.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "windowing", "", "Spectral", 5),
        ParameterSmoothingMode::None);
    (void)parameters_.registerParameter (ParameterDescriptor<float> (kParamSpectrumAnalyzerVST3FFTSize, "FFT Size", "FFT Size",
        ParameterUnit::Samples, ParameterScale::Enum, kParamFlags,
        {0.0f, 2.0f, 1.0f, 0.0f, 1.0f}, nullptr, 0, "fft_size", "samples", "Spectral", 2),
        ParameterSmoothingMode::None);
}

void SpectrumAnalyzerVST3Processor::applyParametersToDSP () noexcept
{
    windowIndex_ = parameters_.getCurrentReal (kParamSpectrumAnalyzerVST3Window);
    fftSizeIndex_ = parameters_.getCurrentReal (kParamSpectrumAnalyzerVST3FFTSize);
    configureAnalyzer ();
}
void SpectrumAnalyzerVST3Processor::configureAnalyzer () noexcept
{
    const int newWindowIndex = std::clamp (static_cast<int> (windowIndex_ + 0.5f), 0, 5);
    const int newFFTSizeIndex = std::clamp (static_cast<int> (fftSizeIndex_ + 0.5f), 0, 2);
    if (newWindowIndex == lastWindowIndex_ && newFFTSizeIndex == lastFFTSizeIndex_)
        return;

    lastWindowIndex_ = newWindowIndex;
    lastFFTSizeIndex_ = newFFTSizeIndex;
    activeFFTSize_ = fftSizeFromIndex (newFFTSizeIndex);
    const auto windowType = windowTypeFromIndex (newWindowIndex);
    (void)analyzer512_.prepare (windowType);
    (void)analyzer1024_.prepare (windowType);
    (void)analyzer2048_.prepare (windowType);
    frameFill_ = 0;
    frame_.fill (0.0f);
    magnitudes_.fill (0.0f);
}

void SpectrumAnalyzerVST3Processor::processAnalysisSample (float sample) noexcept
{
    frame_[frameFill_++] = sample;
    if (frameFill_ < activeFFTSize_)
        return;

    if (activeFFTSize_ == 512)
        analyzer512_.process (frame_.data ());
    else if (activeFFTSize_ == 1024)
        analyzer1024_.process (frame_.data ());
    else
        analyzer2048_.process (frame_.data ());

    updateMagnitudesFromActiveAnalyzer ();
    frameFill_ = 0;
}

void SpectrumAnalyzerVST3Processor::updateMagnitudesFromActiveAnalyzer () noexcept
{
    if (activeFFTSize_ == 512)
    {
        const auto& values = analyzer512_.getMagnitudes ();
        for (std::size_t bin = 0; bin < 256; ++bin)
            magnitudes_[bin] = values[bin];
    }
    else if (activeFFTSize_ == 1024)
    {
        const auto& values = analyzer1024_.getMagnitudes ();
        for (std::size_t bin = 0; bin < 512; ++bin)
            magnitudes_[bin] = values[bin];
    }
    else
    {
        const auto& values = analyzer2048_.getMagnitudes ();
        for (std::size_t bin = 0; bin < 1024; ++bin)
            magnitudes_[bin] = values[bin];
    }
}
void SpectrumAnalyzerVST3Processor::processStereoPassThrough (const Vst::Sample32* leftIn,
                                                     const Vst::Sample32* rightIn,
                                                     Vst::Sample32* leftOut,
                                                     Vst::Sample32* rightOut,
                                                     int32 sampleCount) noexcept
{
    for (int32 sample = 0; sample < sampleCount; ++sample)
    {
        const float left = leftIn[sample];
        const float right = rightIn[sample];
        processAnalysisSample ((left + right) * 0.5f);
        leftOut[sample] = left;
        rightOut[sample] = right;
    }
}

} // namespace CV
