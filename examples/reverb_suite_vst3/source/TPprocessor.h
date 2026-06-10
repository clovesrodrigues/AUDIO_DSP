//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"
#include "CV_DSP/Core/AudioBufferView.hpp"
#include "CV_DSP/Core/ProcessContext.hpp"
#include "CV_DSP/Manager/ParameterManager.hpp"
#include "CV_DSP/Reverb/DeluxeReverb.hpp"
#include "CV_DSP/Reverb/HallReverb.hpp"
#include "CV_DSP/Reverb/PlateReverb.hpp"
#include "CV_DSP/Reverb/RoomReverb.hpp"
#include "CV_DSP/Reverb/SpringReverb.hpp"
#include "CV_DSP/Reverb/SuperReverb.hpp"
#include "CV_DSP/Reverb/TwinReverb.hpp"

#include <cstddef>

namespace CV {

class ReverbSuiteVST3Processor : public Steinberg::Vst::AudioEffect
{
public:
    ReverbSuiteVST3Processor ();
    ~ReverbSuiteVST3Processor () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IAudioProcessor*)new ReverbSuiteVST3Processor;
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
    enum class Algorithm : Steinberg::int32
    {
        Room = 0,
        Hall,
        Plate,
        Spring,
        Twin,
        Deluxe,
        Super,
        Count
    };

    using ParameterManager = cvdsp::manager::ParameterManager<float, 6, 128>;

    void registerParameters () noexcept;
    void prepareDSP () noexcept;
    void resetDSP () noexcept;
    void applyParametersToDSP () noexcept;
    void processSelectedReverb (cvdsp::AudioBufferView<float>& buffer) noexcept;

    cvdsp::reverb::RoomReverb<float> room_ {};
    cvdsp::reverb::HallReverb<float> hall_ {};
    cvdsp::reverb::PlateReverb<float> plate_ {};
    cvdsp::reverb::SpringReverb<float> spring_ {};
    cvdsp::reverb::TwinReverbDSP<float> twin_ {};
    cvdsp::reverb::DeluxeReverbDSP<float> deluxe_ {};
    cvdsp::reverb::SuperReverbDSP<float> super_ {};

    ParameterManager parameters_ {};
    cvdsp::ProcessContext<float> dspContext_ {};

    double sampleRate_ {44100.0};
    Steinberg::int32 algorithm_ {0};
    float mix_ {0.25f};
    float decay_ {0.50f};
    float tone_ {0.60f};
    float preDelayMs_ {20.0f};
    float widthDwell_ {0.60f};
};

} // namespace CV
