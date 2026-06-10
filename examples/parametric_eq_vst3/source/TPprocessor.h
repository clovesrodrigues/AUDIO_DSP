//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/EQ/ParametricEQ.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"

#include <array>

namespace CV {

class ParametricEQVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    ParametricEQVST3Processor ();
    ~ParametricEQVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new ParametricEQVST3Processor;
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
    using EQ = cvdsp::eq::ParametricEQ<float, 5>;
    using ParameterManager = cvdsp::manager::ParameterManager<float, 20, 128>;

    void registerParameters () noexcept;
    void applyParametersToDSP () noexcept;
    void setDefaults () noexcept;
    static cvdsp::filters::BiquadType toBiquadType (Steinberg::int32 typeIndex) noexcept;
    static Steinberg::int32 normalizedToTypeIndex (float normalized) noexcept;
    static Steinberg::Vst::ParamValue typeIndexToNormalized (Steinberg::int32 typeIndex) noexcept;
    static Steinberg::Vst::ParamValue plainToNormalized (float plain, float minPlain, float maxPlain) noexcept;

    std::array<EQ, 2> eqs_ {};
    ParameterManager parameters_ {};
    double sampleRate_ {44100.0};

    float type_[5] {};
    float frequencyHz_[5] {};
    float q_[5] {};
    float gainDB_[5] {};
};

} // namespace CV
