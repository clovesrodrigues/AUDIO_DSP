#ifndef CVDSP_ADAPTERS_VST2_EFFECTBASE_HPP
#define CVDSP_ADAPTERS_VST2_EFFECTBASE_HPP

/**
 * @file VST2EffectBase.hpp
 * @brief Header-only AudioEffectX base class for internal CV_DSP VST2 wrappers.
 */

#include "VST2StateChunk.hpp"

namespace cvdsp::adapters::vst2
{

template<typename T = f32, std::size_t MaxParameters = kDefaultMaxParameters>
class VST2EffectBase : public AudioEffectX
{
public:
    using parameter_state_type = VST2ParameterState<T, MaxParameters>;

    VST2EffectBase(
        audioMasterCallback audioMaster,
        const VstInt32 numPrograms,
        const VstInt32 numParameters,
        const VstInt32 uniqueID,
        const char* effectName,
        const char* vendorName,
        const char* productName,
        const VstInt32 vendorVersion)
        : AudioEffectX(audioMaster, numPrograms, numParameters)
        , vendorVersion_(vendorVersion)
    {
        copyString(effectName_, sizeof(effectName_), effectName);
        copyString(vendorName_, sizeof(vendorName_), vendorName);
        copyString(productName_, sizeof(productName_), productName);

        setNumInputs(2);
        setNumOutputs(2);
        setUniqueID(uniqueID);
        canProcessReplacing(true);
        programsAreChunks(true);
    }

    void setParameter(VstInt32 index, float value) override
    {
        if (index < 0 || !parameters_.setNormalizedByIndex(static_cast<std::size_t>(index), value))
            return;

        const auto* info = parameters_.getInfoByIndex(static_cast<std::size_t>(index));
        if (info != nullptr)
            applyParameterToDSP(info->id, parameters_.getNormalizedByIndex(static_cast<std::size_t>(index)));
    }

    float getParameter(VstInt32 index) override
    {
        return index >= 0
            ? parameters_.getNormalizedByIndex(static_cast<std::size_t>(index), 0.0f)
            : 0.0f;
    }

    void getParameterName(VstInt32 index, char* text) override
    {
        const auto* info = index >= 0 ? parameters_.getInfoByIndex(static_cast<std::size_t>(index)) : nullptr;
        if (info == nullptr || info->descriptor == nullptr)
        {
            copyString(text, kVstMaxParamStrLen, "");
            return;
        }
        copyParameterName(*info->descriptor, text, kVstMaxParamStrLen);
    }

    void getParameterDisplay(VstInt32 index, char* text) override
    {
        const auto* info = index >= 0 ? parameters_.getInfoByIndex(static_cast<std::size_t>(index)) : nullptr;
        if (info == nullptr || info->descriptor == nullptr)
        {
            copyString(text, kVstMaxParamStrLen, "");
            return;
        }
        formatValue(*info->descriptor, getParameter(index), text, kVstMaxParamStrLen);
    }

    void getParameterLabel(VstInt32 index, char* label) override
    {
        const auto* info = index >= 0 ? parameters_.getInfoByIndex(static_cast<std::size_t>(index)) : nullptr;
        if (info == nullptr || info->descriptor == nullptr)
        {
            copyString(label, kVstMaxParamStrLen, "");
            return;
        }
        copyParameterLabel(*info->descriptor, label, kVstMaxParamStrLen);
    }

    VstInt32 getChunk(void** data, bool /*isPreset*/ = false) override
    {
        chunkData_ = makeStateChunk(parameters_);
        *data = chunkData_.empty() ? nullptr : chunkData_.data();
        return static_cast<VstInt32>(chunkData_.size());
    }

    VstInt32 setChunk(void* data, VstInt32 byteSize, bool /*isPreset*/ = false) override
    {
        return restoreStateChunk(parameters_, data, byteSize,
            [this](manager::ParameterID id, NormalizedValue normalized) noexcept {
                applyParameterToDSP(id, normalized);
            })
            ? 1
            : 0;
    }

    void setSampleRate(float sampleRate) override
    {
        AudioEffectX::setSampleRate(sampleRate);
        activeSampleRate_ = sampleRate > 0.0f ? sampleRate : 44100.0f;
        prepareDSP(activeSampleRate_, activeBlockSize_);
    }

    void setBlockSize(VstInt32 blockSize) override
    {
        AudioEffectX::setBlockSize(blockSize);
        activeBlockSize_ = blockSize > 0 ? blockSize : 1024;
        prepareDSP(activeSampleRate_, activeBlockSize_);
    }

    void resume() override
    {
        resetDSP();
        applyAllParametersToDSP();
        AudioEffectX::resume();
    }

    bool getEffectName(char* name) override
    {
        copyString(name, kVstMaxEffectNameLen, effectName_);
        return true;
    }

    bool getVendorString(char* text) override
    {
        copyString(text, kVstMaxVendorStrLen, vendorName_);
        return true;
    }

    bool getProductString(char* text) override
    {
        copyString(text, kVstMaxProductStrLen, productName_);
        return true;
    }

    VstInt32 getVendorVersion() override
    {
        return vendorVersion_;
    }

    VstPlugCategory getPlugCategory() override
    {
        return kPlugCategEffect;
    }

protected:
    template<typename DescriptorArray>
    [[nodiscard]] bool initializeParametersFromDescriptors(const DescriptorArray& descriptors) noexcept
    {
        const bool ok = parameters_.initializeFromDescriptors(descriptors);
        if (ok)
            applyAllParametersToDSP();
        return ok;
    }

    [[nodiscard]] const parameter_state_type& parameterState() const noexcept { return parameters_; }
    [[nodiscard]] parameter_state_type& parameterState() noexcept { return parameters_; }

    void applyAllParametersToDSP() noexcept
    {
        for (std::size_t index = 0; index < parameters_.size(); ++index)
        {
            const auto* info = parameters_.getInfoByIndex(index);
            if (info != nullptr)
                applyParameterToDSP(info->id, parameters_.getNormalizedByIndex(index));
        }
    }

    virtual void applyParameterToDSP(manager::ParameterID id, NormalizedValue normalized) noexcept = 0;
    virtual void prepareDSP(float sampleRate, VstInt32 maxBlockSize) noexcept = 0;
    virtual void resetDSP() noexcept = 0;

private:
    parameter_state_type parameters_ {};
    std::vector<std::uint8_t> chunkData_ {};
    float activeSampleRate_ {44100.0f};
    VstInt32 activeBlockSize_ {1024};
    char effectName_[kVstMaxEffectNameLen] {};
    char vendorName_[kVstMaxVendorStrLen] {};
    char productName_[kVstMaxProductStrLen] {};
    VstInt32 vendorVersion_ {1};
};

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_EFFECTBASE_HPP
