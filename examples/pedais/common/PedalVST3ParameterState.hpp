#pragma once

#include "CV_DSP/Manager/ParameterDescriptor.hpp"

#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace CV::Pedais {

/**
 * @brief Fixed-capacity normalized parameter cache for pedal VST3 processors.
 *
 * The cache stores ParamID/value pairs, consumes VST3 automation queues, and
 * invokes a user callback only when a value changed. It allocates no memory and
 * is intended for setup/control-rate work around the audio hot path.
 */
template<std::size_t MaxParameters>
class PedalVST3ParameterState
{
    static_assert(MaxParameters > 0, "PedalVST3ParameterState requires MaxParameters > 0");

public:
    constexpr PedalVST3ParameterState() noexcept = default;

    template<typename DescriptorArray>
    bool initializeFromDescriptors(const DescriptorArray& descriptors) noexcept
    {
        if (descriptors.size() > MaxParameters)
            return false;

        count_ = 0;
        for (const auto& descriptor : descriptors)
        {
            if (!descriptor.isValid())
                return false;

            using Value = typename DescriptorArray::value_type::value_type;
            ids_[count_] = static_cast<Steinberg::Vst::ParamID>(descriptor.getID());
            values_[count_] = static_cast<Steinberg::Vst::ParamValue>(std::clamp(
                descriptor.realToNormalized(descriptor.getDefaultValue()),
                Value(0),
                Value(1)));
            defaults_[count_] = values_[count_];
            ++count_;
        }

        return true;
    }

    void resetToDefaults() noexcept
    {
        values_ = defaults_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    [[nodiscard]] bool setNormalized(
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::ParamValue normalizedValue) noexcept
    {
        const std::size_t index = findIndex(id);
        if (index >= count_)
            return false;

        values_[index] = clampNormalized(normalizedValue);
        return true;
    }

    [[nodiscard]] Steinberg::Vst::ParamValue getNormalized(
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::ParamValue fallback = 0.0) const noexcept
    {
        const std::size_t index = findIndex(id);
        return index < count_ ? values_[index] : fallback;
    }

    template<typename Callback>
    void applyParameterChanges(Steinberg::Vst::IParameterChanges* changes, Callback&& callback) noexcept
    {
        if (changes == nullptr)
            return;

        const Steinberg::int32 queueCount = changes->getParameterCount();
        for (Steinberg::int32 queueIndex = 0; queueIndex < queueCount; ++queueIndex)
        {
            Steinberg::Vst::IParamValueQueue* queue = changes->getParameterData(queueIndex);
            if (queue == nullptr)
                continue;

            applyQueue(*queue, callback);
        }
    }

private:
    template<typename Callback>
    void applyQueue(Steinberg::Vst::IParamValueQueue& queue, Callback&& callback) noexcept
    {
        const std::size_t index = findIndex(queue.getParameterId());
        if (index >= count_)
            return;

        const Steinberg::int32 pointCount = queue.getPointCount();
        if (pointCount <= 0)
            return;

        Steinberg::int32 sampleOffset = 0;
        Steinberg::Vst::ParamValue normalizedValue = values_[index];
        if (queue.getPoint(pointCount - 1, sampleOffset, normalizedValue) != Steinberg::kResultOk)
            return;

        (void)sampleOffset;
        normalizedValue = clampNormalized(normalizedValue);
        if (normalizedValue == values_[index])
            return;

        values_[index] = normalizedValue;
        callback(ids_[index], normalizedValue);
    }

    [[nodiscard]] std::size_t findIndex(Steinberg::Vst::ParamID id) const noexcept
    {
        for (std::size_t index = 0; index < count_; ++index)
        {
            if (ids_[index] == id)
                return index;
        }
        return count_;
    }

    [[nodiscard]] static Steinberg::Vst::ParamValue clampNormalized(
        Steinberg::Vst::ParamValue value) noexcept
    {
        return std::clamp(value, Steinberg::Vst::ParamValue(0.0), Steinberg::Vst::ParamValue(1.0));
    }

    std::array<Steinberg::Vst::ParamID, MaxParameters> ids_ {};
    std::array<Steinberg::Vst::ParamValue, MaxParameters> values_ {};
    std::array<Steinberg::Vst::ParamValue, MaxParameters> defaults_ {};
    std::size_t count_ {0};
};

} // namespace CV::Pedais
