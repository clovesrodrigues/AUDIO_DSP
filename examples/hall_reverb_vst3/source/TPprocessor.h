//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Core/AudioBufferView.hpp"
#include "CV_DSP/Core/ProcessContext.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"
#include "CV_DSP/Reverb/HallReverb.hpp"

namespace CV {

class HallReverbVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    HallReverbVST3Processor ();
    ~HallReverbVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new HallReverbVST3Processor;
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
    using DSP = cvdsp::reverb::HallReverb<float>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 4, 128>;

    void registerParameters () noexcept;
    void prepareDSP () noexcept;
    void resetDSP () noexcept;
    void applyParametersToDSP () noexcept;

    DSP dsp_ {};
    ParameterManager parameters_ {};
    cvdsp::ProcessContext<float> dspContext_ {};

    double sampleRate_ {44100.0};
    float mix_ {0.25f};
    float decay_ {0.50f};
    float damping_ {0.35f};
    float preDelayMs_ {20.0f};
};

} // namespace CV
