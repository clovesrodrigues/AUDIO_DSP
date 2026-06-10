//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Spatial/StereoWidth.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

namespace CV {

class StereoWidthVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    StereoWidthVST3Processor ();
    ~StereoWidthVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new StereoWidthVST3Processor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

protected:
    using DSP = cvdsp::spatial::StereoWidth<float>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 3, 128>;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;
    void processStereoBlock (const Steinberg::Vst::Sample32* leftIn,
                             const Steinberg::Vst::Sample32* rightIn,
                             Steinberg::Vst::Sample32* leftOut,
                             Steinberg::Vst::Sample32* rightOut,
                             Steinberg::int32 sampleCount) noexcept;

    DSP processor_ {};
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};
    float midGain_ {1.0f};
    float sideGain_ {1.0f};
    float width_ {1.0f};
};

} // namespace CV
