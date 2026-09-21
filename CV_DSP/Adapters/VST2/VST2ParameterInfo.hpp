#ifndef CVDSP_ADAPTERS_VST2_PARAMETERINFO_HPP
#define CVDSP_ADAPTERS_VST2_PARAMETERINFO_HPP

/**
 * @file VST2ParameterInfo.hpp
 * @brief VST2-facing metadata derived from CV_DSP neutral descriptors.
 */

#include "VST2Config.hpp"
#include "../../Manager/ParameterDescriptor.hpp"

#include <cmath>
#include <cstdio>

namespace cvdsp::adapters::vst2
{

template<typename T = f32>
struct VST2ParameterInfo
{
    using descriptor_type = manager::ParameterDescriptor<T>;

    ParameterIndex index = 0;
    manager::ParameterID id = 0;
    const descriptor_type* descriptor = nullptr;
    NormalizedValue defaultNormalized = 0.0f;

    [[nodiscard]] bool isValid() const noexcept
    {
        return descriptor != nullptr && descriptor->isValid();
    }
};

[[nodiscard]] inline const char* unitLabelForUnit(
    const manager::ParameterUnit unit,
    const char* explicitUnitLabel) noexcept
{
    if (explicitUnitLabel != nullptr && explicitUnitLabel[0] != '\0')
        return explicitUnitLabel;

    switch (unit)
    {
        case manager::ParameterUnit::Hertz: return "Hz";
        case manager::ParameterUnit::Milliseconds: return "ms";
        case manager::ParameterUnit::Seconds: return "s";
        case manager::ParameterUnit::Decibels: return "dB";
        case manager::ParameterUnit::Percent: return "%";
        case manager::ParameterUnit::Ratio: return "x";
        case manager::ParameterUnit::Semitones: return "st";
        case manager::ParameterUnit::Cents: return "ct";
        case manager::ParameterUnit::Degrees: return "deg";
        case manager::ParameterUnit::Samples: return "spl";
        case manager::ParameterUnit::Beats: return "beat";
        case manager::ParameterUnit::BPM: return "BPM";
        case manager::ParameterUnit::None:
        case manager::ParameterUnit::Index:
        case manager::ParameterUnit::Custom:
        default:
            return "";
    }
}

template<typename T>
[[nodiscard]] inline VstInt32 stepCountForDescriptor(
    const manager::ParameterDescriptor<T>& descriptor) noexcept
{
    if (descriptor.getScale() == manager::ParameterScale::Boolean
        || descriptor.hasFlag(manager::ParameterFlag::Boolean))
        return 1;

    if (descriptor.getScale() == manager::ParameterScale::Enum
        || descriptor.hasFlag(manager::ParameterFlag::Enum))
    {
        const std::size_t count = descriptor.getEnumEntryCount();
        return count > 1 ? static_cast<VstInt32>(count - 1) : 0;
    }

    const auto& range = descriptor.getRange();
    if (range.hasStep())
    {
        const double span = static_cast<double>(range.maximum - range.minimum);
        const double step = static_cast<double>(range.step);
        if (span > 0.0 && step > 0.0)
            return static_cast<VstInt32>((span / step) + 0.5);
    }

    if (descriptor.hasFlag(manager::ParameterFlag::Discrete))
    {
        const double span = static_cast<double>(range.maximum - range.minimum);
        if (span > 0.0)
            return static_cast<VstInt32>(span + 0.5);
    }

    return 0;
}

template<typename T>
[[nodiscard]] inline NormalizedValue defaultNormalizedForDescriptor(
    const manager::ParameterDescriptor<T>& descriptor) noexcept
{
    const T normalized = descriptor.realToNormalized(descriptor.getDefaultValue());
    return static_cast<NormalizedValue>(std::clamp(normalized, T(0), T(1)));
}

template<typename T>
void formatValue(
    const manager::ParameterDescriptor<T>& descriptor,
    const NormalizedValue normalized,
    char* destination,
    const std::size_t capacity) noexcept
{
    if (destination == nullptr || capacity == 0)
        return;

    const T normalizedValue = std::clamp(static_cast<T>(normalized), T(0), T(1));
    const T realValue = descriptor.normalizedToReal(normalizedValue);

    if (descriptor.getScale() == manager::ParameterScale::Boolean
        || descriptor.hasFlag(manager::ParameterFlag::Boolean))
    {
        copyString(destination, capacity, realValue >= T(0.5) ? "On" : "Off");
        return;
    }

    if (descriptor.getScale() == manager::ParameterScale::Enum
        || descriptor.hasFlag(manager::ParameterFlag::Enum))
    {
        const auto index = static_cast<std::size_t>(std::max<T>(T(0), std::round(realValue)));
        const auto* entry = descriptor.getEnumEntry(index);
        copyString(destination, capacity, entry != nullptr ? entry->label : "");
        return;
    }

    const unsigned precision = descriptor.getDisplayPrecision();
    const unsigned boundedPrecision = precision > 4u ? 4u : precision;

    switch (boundedPrecision)
    {
        case 0: std::snprintf(destination, capacity, "%.0f", static_cast<double>(realValue)); break;
        case 1: std::snprintf(destination, capacity, "%.1f", static_cast<double>(realValue)); break;
        case 2: std::snprintf(destination, capacity, "%.2f", static_cast<double>(realValue)); break;
        case 3: std::snprintf(destination, capacity, "%.3f", static_cast<double>(realValue)); break;
        default: std::snprintf(destination, capacity, "%.4f", static_cast<double>(realValue)); break;
    }
}

template<typename T>
void copyParameterName(const manager::ParameterDescriptor<T>& descriptor, char* destination, std::size_t capacity) noexcept
{
    copyString(destination, capacity, descriptor.getShortName());
}

template<typename T>
void copyParameterLabel(const manager::ParameterDescriptor<T>& descriptor, char* destination, std::size_t capacity) noexcept
{
    copyString(destination, capacity, unitLabelForUnit(descriptor.getUnit(), descriptor.getUnitLabel()));
}

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_PARAMETERINFO_HPP
