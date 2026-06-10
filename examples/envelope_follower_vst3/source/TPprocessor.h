//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Dynamics/EnvelopeFollower.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

#include <array>

namespace CV {

class EnvelopeFollowerVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    EnvelopeFollowerVST3Processor ();
    ~EnvelopeFollowerVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new EnvelopeFollowerVST3Processor;
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
    using DSP = cvdsp::dynamics::EnvelopeFollower<float>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 5, 128>;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;
    static Steinberg::int32 normalizedToModeIndex (float normalized) noexcept;
    static Steinberg::Vst::ParamValue modeIndexToNormalized (Steinberg::int32 modeIndex) noexcept;
    static float dbToLinear (float db) noexcept;

    std::array<DSP, 2> followers_ {};
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};
    Steinberg::int32 modeIndex_ {0};
    float attackMs_ {10.0f};
    float releaseMs_ {100.0f};
    float outputGainDB_ {0.0f};
    float mixPercent_ {100.0f};
};

} // namespace CV
