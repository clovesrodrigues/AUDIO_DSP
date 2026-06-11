//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "CV_DSP/Spectral/RealtimeNoiseReducer.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <array>
#include <cstddef>

namespace CV {

class SpectralNoiseReducerVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    SpectralNoiseReducerVST3Processor ();
    ~SpectralNoiseReducerVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new SpectralNoiseReducerVST3Processor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
    using DSP = cvdsp::spectral::RealtimeNoiseReducer<float, 1024>;

    static float clamp01 (double value) noexcept;
    static float normalizedToOutputGainDb (double normalized) noexcept;
    static void clearRemainingOutputs (Steinberg::Vst::ProcessData& data, Steinberg::int32 firstBus) noexcept;
    static void copyInputsToOutputs (Steinberg::Vst::ProcessData& data) noexcept;

    void resetParametersToDefaults () noexcept;
    void applyAllParametersToDSP () noexcept;
    void applyParameterToDSP (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue normalizedValue) noexcept;
    Steinberg::Vst::ParamValue getParameterNormalized (Steinberg::Vst::ParamID id) const noexcept;
    void setParameterNormalized (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue normalizedValue) noexcept;

    std::array<DSP, 2> processors_ {};
    std::array<Steinberg::Vst::ParamValue, 5> parameters_ {};
    double sampleRate_ {44100.0};
    bool learnNoiseActive_ {false};
    bool subtractNoiseActive_ {false};
};

} // namespace CV
