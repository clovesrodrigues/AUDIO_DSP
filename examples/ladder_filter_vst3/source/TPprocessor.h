//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Filters/LadderFilter.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

#include <array>

namespace CV {

class LadderFilterVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    LadderFilterVST3Processor ();
    ~LadderFilterVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new LadderFilterVST3Processor;
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
    using DSP = cvdsp::filters::LadderFilter<float>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 3, 128>;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;

    std::array<DSP, 2> processors_ {};
    
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};
    float cutoffHz_ {1000.0f};
    float resonance_ {0.0f};
    float drive_ {1.0f};
};

} // namespace CV
