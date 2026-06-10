#pragma once

#include "CV_DSP/Manager/ParameterDescriptor.hpp"

#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace CV::Pedais {

/**
 * @brief VST3-ready parameter metadata derived from one CV_DSP descriptor.
 *
 * Controllers can pass this data directly to Steinberg's ParameterContainer:
 * parameters.addParameter(def.title, def.units, def.stepCount,
 *                         def.defaultNormalizedValue, def.flags,
 *                         def.id, def.unitID, def.shortTitle);
 */
struct PedalVST3ParameterDefinition
{
    Steinberg::Vst::String128 title {};
    Steinberg::Vst::String128 shortTitle {};
    Steinberg::Vst::String128 units {};
    Steinberg::Vst::ParamID id {0};
    Steinberg::Vst::UnitID unitID {Steinberg::Vst::kRootUnitId};
    Steinberg::int32 stepCount {0};
    Steinberg::Vst::ParamValue defaultNormalizedValue {0.0};
    Steinberg::int32 flags {Steinberg::Vst::ParameterInfo::kNoFlags};
};

inline void copyAsciiToString128(Steinberg::Vst::String128 destination, const char* source) noexcept
{
    constexpr std::size_t kMaxChars = 127;
    std::size_t index = 0;

    if (source != nullptr)
    {
        for (; index < kMaxChars && source[index] != '\0'; ++index)
            destination[index] = static_cast<Steinberg::Vst::TChar>(source[index]);
    }

    destination[index] = static_cast<Steinberg::Vst::TChar>(0);
}

[[nodiscard]] inline const char* unitLabelForDescriptor(
    const cvdsp::manager::ParameterUnit unit,
    const char* explicitUnitLabel) noexcept
{
    if (explicitUnitLabel != nullptr && explicitUnitLabel[0] != '\0')
        return explicitUnitLabel;

    using Unit = cvdsp::manager::ParameterUnit;
    switch (unit)
    {
        case Unit::Hertz: return "Hz";
        case Unit::Milliseconds: return "ms";
        case Unit::Seconds: return "s";
        case Unit::Decibels: return "dB";
        case Unit::Percent: return "%";
        case Unit::Ratio: return "x";
        case Unit::Semitones: return "st";
        case Unit::Cents: return "ct";
        case Unit::Degrees: return "deg";
        case Unit::Samples: return "samples";
        case Unit::Beats: return "beats";
        case Unit::BPM: return "BPM";
        case Unit::None:
        case Unit::Index:
        case Unit::Custom:
        default: return "";
    }
}

[[nodiscard]] inline Steinberg::int32 flagsForDescriptor(
    const cvdsp::manager::ParameterFlags flags) noexcept
{
    using Flag = cvdsp::manager::ParameterFlag;

    Steinberg::int32 vstFlags = Steinberg::Vst::ParameterInfo::kNoFlags;

    if (cvdsp::manager::hasFlag(flags, Flag::Automatable)
        && !cvdsp::manager::hasFlag(flags, Flag::ReadOnly)
        && !cvdsp::manager::hasFlag(flags, Flag::Hidden))
    {
        vstFlags |= Steinberg::Vst::ParameterInfo::kCanAutomate;
    }

    if (cvdsp::manager::hasFlag(flags, Flag::ReadOnly))
        vstFlags |= Steinberg::Vst::ParameterInfo::kIsReadOnly;

    if (cvdsp::manager::hasFlag(flags, Flag::Hidden))
    {
        vstFlags |= Steinberg::Vst::ParameterInfo::kIsHidden
            | Steinberg::Vst::ParameterInfo::kIsReadOnly;
    }

    if (cvdsp::manager::hasFlag(flags, Flag::Enum))
        vstFlags |= Steinberg::Vst::ParameterInfo::kIsList;

    if (cvdsp::manager::hasFlag(flags, Flag::Bypass))
        vstFlags |= Steinberg::Vst::ParameterInfo::kIsBypass;

    return vstFlags;
}

template<typename T>
[[nodiscard]] Steinberg::int32 stepCountForDescriptor(
    const cvdsp::manager::ParameterDescriptor<T>& descriptor) noexcept
{
    using Scale = cvdsp::manager::ParameterScale;
    using Flag = cvdsp::manager::ParameterFlag;

    if (descriptor.getScale() == Scale::Boolean || descriptor.hasFlag(Flag::Boolean))
        return 1;

    if (descriptor.getScale() == Scale::Enum || descriptor.hasFlag(Flag::Enum))
    {
        const std::size_t count = descriptor.getEnumEntryCount();
        return count > 1
            ? static_cast<Steinberg::int32>(std::min<std::size_t>(
                  count - 1,
                  static_cast<std::size_t>(std::numeric_limits<Steinberg::int32>::max())))
            : 0;
    }

    const auto& range = descriptor.getRange();
    if (range.hasStep())
    {
        const double span = static_cast<double>(range.maximum - range.minimum);
        const double step = static_cast<double>(range.step);
        if (span > 0.0 && step > 0.0)
        {
            const auto steps = static_cast<std::size_t>((span / step) + 0.5);
            return static_cast<Steinberg::int32>(std::min<std::size_t>(
                steps,
                static_cast<std::size_t>(std::numeric_limits<Steinberg::int32>::max())));
        }
    }

    if (descriptor.hasFlag(Flag::Discrete))
    {
        const double span = static_cast<double>(range.maximum - range.minimum);
        if (span > 0.0)
        {
            const auto steps = static_cast<std::size_t>(span + 0.5);
            return static_cast<Steinberg::int32>(std::min<std::size_t>(
                steps,
                static_cast<std::size_t>(std::numeric_limits<Steinberg::int32>::max())));
        }
    }

    return 0;
}

template<typename T>
[[nodiscard]] Steinberg::Vst::ParamValue defaultNormalizedForDescriptor(
    const cvdsp::manager::ParameterDescriptor<T>& descriptor) noexcept
{
    const T normalized = descriptor.realToNormalized(descriptor.getDefaultValue());
    return static_cast<Steinberg::Vst::ParamValue>(std::clamp(
        normalized,
        static_cast<T>(0),
        static_cast<T>(1)));
}

template<typename T>
[[nodiscard]] PedalVST3ParameterDefinition makeParameterDefinition(
    const cvdsp::manager::ParameterDescriptor<T>& descriptor) noexcept
{
    PedalVST3ParameterDefinition definition {};
    copyAsciiToString128(definition.title, descriptor.getLongName());
    copyAsciiToString128(definition.shortTitle, descriptor.getShortName());
    copyAsciiToString128(
        definition.units,
        unitLabelForDescriptor(descriptor.getUnit(), descriptor.getUnitLabel()));

    definition.id = static_cast<Steinberg::Vst::ParamID>(descriptor.getID());
    definition.stepCount = stepCountForDescriptor(descriptor);
    definition.defaultNormalizedValue = defaultNormalizedForDescriptor(descriptor);
    definition.flags = flagsForDescriptor(descriptor.getFlags());
    return definition;
}

template<typename DescriptorArray, typename Callback>
void forEachParameterDefinition(const DescriptorArray& descriptors, Callback&& callback) noexcept
{
    for (const auto& descriptor : descriptors)
    {
        if (!descriptor.isValid() || descriptor.hasFlag(cvdsp::manager::ParameterFlag::Hidden))
            continue;

        const auto definition = makeParameterDefinition(descriptor);
        callback(definition);
    }
}

template<typename DescriptorArray>
[[nodiscard]] std::size_t countVisibleParameters(const DescriptorArray& descriptors) noexcept
{
    std::size_t count = 0;
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.isValid() && !descriptor.hasFlag(cvdsp::manager::ParameterFlag::Hidden))
            ++count;
    }
    return count;
}

template<typename ParameterContainer, typename DescriptorArray>
[[nodiscard]] std::size_t addDefinitionsToParameterContainer(
    ParameterContainer& parameters,
    const DescriptorArray& descriptors) noexcept
{
    std::size_t added = 0;
    forEachParameterDefinition(descriptors, [&parameters, &added](
        const PedalVST3ParameterDefinition& definition) noexcept {
        if (parameters.addParameter(
                definition.title,
                definition.units,
                definition.stepCount,
                definition.defaultNormalizedValue,
                definition.flags,
                definition.id,
                definition.unitID,
                definition.shortTitle) != nullptr)
        {
            ++added;
        }
    });
    return added;
}

} // namespace CV::Pedais
