//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/vst/vstaudioeffect.h"
#include "CV_DSP/Dynamics/Compressor.hpp"

#include <array>

namespace CV {

//------------------------------------------------------------------------
//  CompressorVST3Processor
//------------------------------------------------------------------------
class CompressorVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    CompressorVST3Processor ();
    ~CompressorVST3Processor () SMTG_OVERRIDE;

    // Create function
    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new CompressorVST3Processor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

protected:
    using Compressor = cvdsp::dynamics::Compressor<float>;

    void applyParameters () noexcept;
    void setDetectorFromIndex (Steinberg::int32 detectorIndex) noexcept;

    static float normalizedToPlain (Steinberg::Vst::ParamValue normalized, float minPlain, float maxPlain) noexcept;
    static Steinberg::Vst::ParamValue plainToNormalized (float plain, float minPlain, float maxPlain) noexcept;
    static Steinberg::int32 normalizedToDetectorIndex (Steinberg::Vst::ParamValue normalized) noexcept;
    static Steinberg::Vst::ParamValue detectorIndexToNormalized (Steinberg::int32 detectorIndex) noexcept;

    std::array<Compressor, 2> compressors_ {};
    double sampleRate_ {44100.0};

    float thresholdDB_ {-18.0f};
    float ratio_ {4.0f};
    float attackMs_ {10.0f};
    float releaseMs_ {100.0f};
    float kneeDB_ {6.0f};
    float makeupDB_ {3.0f};
    Steinberg::int32 detectorIndex_ {0};
};

//------------------------------------------------------------------------
} // namespace CV
