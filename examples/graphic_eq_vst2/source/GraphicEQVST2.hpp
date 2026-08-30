#pragma once

#include "CV_DSP/Adapters/VST2/VST2EffectBase.hpp"
#include "CV_DSP/EQ/GraphicEQ.hpp"
#include "CV_DSP/Manager/ParameterDescriptor.hpp"

#include <array>
#include <cstddef>

namespace CV {

class GraphicEQVST2 final : public cvdsp::adapters::vst2::VST2EffectBase<float, 10>
{
public:
    explicit GraphicEQVST2(audioMasterCallback audioMaster);

    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;

private:
    static constexpr std::size_t kBandCount = 10;
    using EQ = cvdsp::eq::GraphicEQ<float, kBandCount>;
    using Descriptor = cvdsp::manager::ParameterDescriptor<float>;
    using DescriptorArray = std::array<Descriptor, kBandCount>;

    static constexpr VstInt32 kNumPrograms = 1;
    static constexpr VstInt32 kUniqueID = CCONST('C', 'v', 'G', 'e');
    static constexpr VstInt32 kVendorVersion = 1000;

    static const DescriptorArray& descriptors() noexcept;
    static float normalizedToReal(
        cvdsp::manager::ParameterID id,
        cvdsp::adapters::vst2::NormalizedValue normalizedValue) noexcept;

    void applyParameterToDSP(
        cvdsp::manager::ParameterID id,
        cvdsp::adapters::vst2::NormalizedValue normalized) noexcept override;
    void prepareDSP(float sampleRate, VstInt32 maxBlockSize) noexcept override;
    void resetDSP() noexcept override;

    std::array<EQ, 2> eqs_ {};
    std::array<float, kBandCount> gainsDB_ {};
};

} // namespace CV
