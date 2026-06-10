//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Guitar/TubePreamp.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

#include <array>
#include <cstddef>

namespace CV {

class TubePreampVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    TubePreampVST3Processor ();
    ~TubePreampVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new TubePreampVST3Processor;
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
    using DSP = cvdsp::TubePreamp<float>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 5, 128>;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;
    float processSample (float input, std::size_t channel) noexcept;
    static std::size_t toStageCount (float plain) noexcept;

    std::array<DSP, 2> processors_ {};
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};
    float drive_ {4.0f};
    float bias_ {0.0f};
    float plateVoltage_ {250.0f};
    float stages_ {2.0f};
    float outputGain_ {1.0f};
};

} // namespace CV
