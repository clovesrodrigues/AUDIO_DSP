//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Spectral/FFT.hpp"
#include "CV_DSP/Spectral/WindowFunctions.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

#include <array>
#include <cstddef>

namespace CV {

class FFTVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    FFTVST3Processor ();
    ~FFTVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new FFTVST3Processor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

protected:
    using DSP = cvdsp::spectral::FFT<float>;
    using Complex = DSP::Complex;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 2, 128>;
    static constexpr std::size_t MaxFFTSize = 2048;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;
    void configureAnalyzer () noexcept;
    void processAnalysisSample (float sample) noexcept;
    void processStereoPassThrough (const Steinberg::Vst::Sample32* leftIn,
                                   const Steinberg::Vst::Sample32* rightIn,
                                   Steinberg::Vst::Sample32* leftOut,
                                   Steinberg::Vst::Sample32* rightOut,
                                   Steinberg::int32 sampleCount) noexcept;

    DSP fft_ {};
    std::array<float, MaxFFTSize> frame_ {};
    std::array<float, MaxFFTSize> window_ {};
    std::array<Complex, MaxFFTSize> spectrum_ {};
    std::array<float, MaxFFTSize / 2> magnitudes_ {};
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};
    std::size_t fftSize_ {1024};
    std::size_t frameFill_ {0};
    float windowIndex_ {1.0f};
    float fftSizeIndex_ {1.0f};
    int lastWindowIndex_ {-1};
    int lastFFTSizeIndex_ {-1};
};

} // namespace CV
