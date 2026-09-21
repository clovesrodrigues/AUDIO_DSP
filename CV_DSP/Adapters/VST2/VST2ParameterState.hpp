#ifndef CVDSP_ADAPTERS_VST2_PARAMETERSTATE_HPP
#define CVDSP_ADAPTERS_VST2_PARAMETERSTATE_HPP

/**
 * @file VST2ParameterState.hpp
 * @brief Fixed-capacity normalized parameter cache for VST2 wrappers.
 */

#include "VST2ParameterInfo.hpp"

namespace cvdsp::adapters::vst2
{

template<typename T = f32, std::size_t MaxParameters = kDefaultMaxParameters>
class VST2ParameterState
{
public:
    using descriptor_type = manager::ParameterDescriptor<T>;
    using info_type = VST2ParameterInfo<T>;

    static_assert(MaxParameters > 0, "VST2ParameterState requires MaxParameters > 0");

    constexpr VST2ParameterState() noexcept = default;

    template<typename DescriptorArray>
    [[nodiscard]] bool initializeFromDescriptors(const DescriptorArray& descriptors) noexcept
    {
        if (descriptors.size() > MaxParameters)
            return false;

        count_ = 0;
        for (const auto& descriptor : descriptors)
        {
            if (!addDescriptor(descriptor))
                return false;
        }
        return true;
    }

    [[nodiscard]] bool addDescriptor(const descriptor_type& descriptor) noexcept
    {
        if (!descriptor.isValid() || count_ >= MaxParameters)
            return false;

        descriptors_[count_] = descriptor;
        infos_[count_].index = static_cast<ParameterIndex>(count_);
        infos_[count_].id = descriptors_[count_].getID();
        infos_[count_].descriptor = descriptors_.data() + count_;
        infos_[count_].defaultNormalized = defaultNormalizedForDescriptor(descriptors_[count_]);
        values_[count_] = infos_[count_].defaultNormalized;
        defaults_[count_] = infos_[count_].defaultNormalized;
        ++count_;
        return true;
    }

    void resetToDefaults() noexcept
    {
        for (std::size_t index = 0; index < count_; ++index)
            values_[index] = defaults_[index];
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return MaxParameters; }

    [[nodiscard]] const info_type* getInfoByIndex(const std::size_t index) const noexcept
    {
        return index < count_ ? infos_.data() + index : nullptr;
    }

    [[nodiscard]] const info_type* getInfoByID(const manager::ParameterID id) const noexcept
    {
        const std::size_t index = findIndexByID(id);
        return index < count_ ? getInfoByIndex(index) : nullptr;
    }

    [[nodiscard]] std::size_t findIndexByID(const manager::ParameterID id) const noexcept
    {
        for (std::size_t index = 0; index < count_; ++index)
        {
            if (infos_[index].id == id)
                return index;
        }
        return count_;
    }

    [[nodiscard]] bool setNormalizedByIndex(const std::size_t index, const NormalizedValue value) noexcept
    {
        if (index >= count_)
            return false;

        values_[index] = clampNormalized(value);
        return true;
    }

    [[nodiscard]] bool setNormalizedByID(const manager::ParameterID id, const NormalizedValue value) noexcept
    {
        const std::size_t index = findIndexByID(id);
        return index < count_ && setNormalizedByIndex(index, value);
    }

    [[nodiscard]] NormalizedValue getNormalizedByIndex(
        const std::size_t index,
        const NormalizedValue fallback = 0.0f) const noexcept
    {
        return index < count_ ? values_[index] : fallback;
    }

    [[nodiscard]] NormalizedValue getNormalizedByID(
        const manager::ParameterID id,
        const NormalizedValue fallback = 0.0f) const noexcept
    {
        const std::size_t index = findIndexByID(id);
        return index < count_ ? values_[index] : fallback;
    }

private:
    [[nodiscard]] static NormalizedValue clampNormalized(const NormalizedValue value) noexcept
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    std::array<info_type, MaxParameters> infos_ {};
    std::array<descriptor_type, MaxParameters> descriptors_ {};
    std::array<NormalizedValue, MaxParameters> values_ {};
    std::array<NormalizedValue, MaxParameters> defaults_ {};
    std::size_t count_ {0};
};

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_PARAMETERSTATE_HPP
