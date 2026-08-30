#pragma once

#include "CV_DSP/Adapters/VST2/VST2EffectBase.hpp"
#include "CV_DSP/Guitar/Pedals/PedalParameterIDs.hpp"
#include "CV_DSP/Guitar/Pedals/SustainerDSP.hpp"

#include <array>
#include <cstddef>

namespace CV {

class SustainerVST2 final : public cvdsp::adapters::vst2::VST2EffectBase<float, 15>
{
public:
    explicit SustainerVST2(audioMasterCallback audioMaster);

    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;

private:
    using DSP = cvdsp::guitar::pedals::SustainerDSP<float>;
    using DescriptorArray = DSP::DescriptorArray;

    static constexpr VstInt32 kNumPrograms = 1;
    static constexpr VstInt32 kUniqueID = CCONST('C', 'v', 'S', 'u');
    static constexpr VstInt32 kVendorVersion = 1000;

    static const DescriptorArray& descriptors() noexcept;
    static float normalizedToReal(cvdsp::manager::ParameterID id, cvdsp::adapters::vst2::NormalizedValue normalizedValue) noexcept;
    static int normalizedToIndex(cvdsp::manager::ParameterID id, cvdsp::adapters::vst2::NormalizedValue normalizedValue, int maxIndex) noexcept;

    void applyParameterToDSP(cvdsp::manager::ParameterID id, cvdsp::adapters::vst2::NormalizedValue normalized) noexcept override;
    void prepareDSP(float sampleRate, VstInt32 maxBlockSize) noexcept override;
    void resetDSP() noexcept override;

    static void copyOrClear(float* output, const float* input, VstInt32 sampleFrames) noexcept;

    std::array<DSP, 2> processors_ {};
    float sampleRate_ {44100.0f};
    VstInt32 maxBlockSize_ {1024};
};

} // namespace CV

