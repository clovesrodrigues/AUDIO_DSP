//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "CV_DSP/Guitar/Pedals/ChainsawMetalDSP.hpp"
#include "examples/pedais/common/PedalVST3ParameterAdapter.hpp"
#include "examples/pedais/common/PedalVST3ParameterState.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <array>
#include <cstddef>
#include <tuple>

namespace CV {

class ChainsawMetalVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    ChainsawMetalVST3Processor ();
    ~ChainsawMetalVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new ChainsawMetalVST3Processor;
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
    using DSP = cvdsp::guitar::pedals::ChainsawMetalDSP<float>;
    using DescriptorArray = DSP::DescriptorArray;
    static constexpr std::size_t kParameterCount = std::tuple_size_v<DescriptorArray>;
    using ParameterState = CV::Pedais::PedalVST3ParameterState<kParameterCount>;

    static const DescriptorArray& descriptors () noexcept;
    void applyAllParametersToDSP () noexcept;
    void applyParameterToDSP (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue normalizedValue) noexcept;
    static float normalizedToReal (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue normalizedValue) noexcept;
    static int normalizedToIndex (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue normalizedValue, int maxIndex) noexcept;
    static void clearRemainingOutputs (Steinberg::Vst::ProcessData& data, Steinberg::int32 firstBus) noexcept;

    std::array<DSP, 2> processors_ {};
    ParameterState parameterState_ {};
    double sampleRate_ {44100.0};
};

} // namespace CV
