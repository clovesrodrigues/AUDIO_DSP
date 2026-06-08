#ifndef CVDSP_MANAGER_PARAMETERDESCRIPTOR_HPP
#define CVDSP_MANAGER_PARAMETERDESCRIPTOR_HPP

/**
 * @file ParameterDescriptor.hpp
 * @brief Neutral parameter metadata and value conversion for CV_DSP.
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * This header defines the immutable metadata used by the future CV_DSP
 * parameter-management layer. It intentionally describes parameters without
 * owning runtime state, smoothing state, automation queues, preset storage, UI
 * objects, or host-SDK objects. The descriptor can therefore be shared by VST3,
 * JUCE, CLAP, iPlug2, and standalone integrations through thin adapters.
 *
 * The descriptor performs deterministic normalized-value conversion and basic
 * validation using only CV_DSP scalar aliases and standard-library facilities.
 * It does not allocate memory, throw exceptions, use RTTI, or depend on any
 * external framework. Simple accessors are suitable for audio hot paths; scale
 * conversions that use logarithmic or exponential math are allocation-free but
 * are intended primarily for parameter-event, setup, or cached state updates.
 */

#include "../Core/Namespace.hpp"
#include "../Core/Types.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace cvdsp::manager
{

/**
 * @brief Stable neutral parameter identifier used by CV_DSP managers.
 *
 * Host-specific identifiers, such as VST3 ParamID values, should be mapped to
 * this neutral ID by adapter layers instead of being stored in descriptors.
 */
using ParameterID = u32;

/**
 * @brief Storage type used for ParameterFlag bit masks.
 */
using ParameterFlags = u32;

/**
 * @brief Parameter conversion scale used for normalized-value mapping.
 */
enum class ParameterScale : u32
{
    Linear = 0,       ///< Uniform interpolation between minimum and maximum.
    Logarithmic,      ///< Log-domain interpolation for strictly positive ranges.
    Exponential,      ///< Linear range with configurable normalized exponent.
    Decibel,          ///< Linear interpolation in dB units.
    Percentage,       ///< Linear percentage-style control whose real range is 0..1.
    Boolean,          ///< Two-state thresholded control.
    Enum              ///< Discrete indexed control.
};

/**
 * @brief Unit hint for host/UI formatting and adapter metadata.
 */
enum class ParameterUnit : u32
{
    None = 0,
    Hertz,
    Milliseconds,
    Seconds,
    Decibels,
    Percent,
    Ratio,
    Semitones,
    Cents,
    Degrees,
    Samples,
    Beats,
    BPM,
    Index,
    Custom
};

/**
 * @brief High-level value category used by managers and adapters.
 */
enum class ParameterValueKind : u32
{
    Continuous = 0,
    Discrete,
    Boolean,
    Enum
};

/**
 * @brief Bit flags that describe host, preset, and modulation behavior.
 */
enum class ParameterFlag : ParameterFlags
{
    None        = 0u,
    Automatable = 1u << 0u, ///< The host may automate this parameter.
    Modulatable = 1u << 1u, ///< The modulation matrix may target this parameter.
    Persistent  = 1u << 2u, ///< Preset/state systems should store this parameter.
    Hidden      = 1u << 3u, ///< Host/UI adapters should not expose this parameter.
    ReadOnly    = 1u << 4u, ///< The parameter is observable but not editable.
    Discrete    = 1u << 5u, ///< The real value is quantized to steps or indices.
    Boolean     = 1u << 6u, ///< The parameter represents false/true state.
    Enum        = 1u << 7u, ///< The parameter represents an indexed option list.
    Bypass      = 1u << 8u, ///< The parameter represents bypass behavior.
    PerVoice    = 1u << 9u, ///< The parameter may be addressed per voice/note.
    Meta        = 1u << 10u ///< The parameter controls metadata or routing behavior.
};

/**
 * @brief Converts a ParameterFlag enumerator to its mask representation.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags toMask(
    const ParameterFlag flag) noexcept
{
    return static_cast<ParameterFlags>(flag);
}

/**
 * @brief Mask containing every currently defined ParameterFlag bit.
 */
inline constexpr ParameterFlags ValidParameterFlags =
    toMask(ParameterFlag::Automatable)
    | toMask(ParameterFlag::Modulatable)
    | toMask(ParameterFlag::Persistent)
    | toMask(ParameterFlag::Hidden)
    | toMask(ParameterFlag::ReadOnly)
    | toMask(ParameterFlag::Discrete)
    | toMask(ParameterFlag::Boolean)
    | toMask(ParameterFlag::Enum)
    | toMask(ParameterFlag::Bypass)
    | toMask(ParameterFlag::PerVoice)
    | toMask(ParameterFlag::Meta);

/**
 * @brief Combines two ParameterFlag enumerators into a mask.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags operator|(
    const ParameterFlag lhs,
    const ParameterFlag rhs) noexcept
{
    return toMask(lhs) | toMask(rhs);
}

/**
 * @brief Combines an existing mask with a ParameterFlag enumerator.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags operator|(
    const ParameterFlags lhs,
    const ParameterFlag rhs) noexcept
{
    return lhs | toMask(rhs);
}

/**
 * @brief Intersects two ParameterFlag enumerators into a mask.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags operator&(
    const ParameterFlag lhs,
    const ParameterFlag rhs) noexcept
{
    return toMask(lhs) & toMask(rhs);
}

/**
 * @brief Intersects an existing mask with a ParameterFlag enumerator.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags operator&(
    const ParameterFlags lhs,
    const ParameterFlag rhs) noexcept
{
    return lhs & toMask(rhs);
}

/**
 * @brief Returns true when a flag mask contains the requested flag.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool hasFlag(
    const ParameterFlags flags,
    const ParameterFlag flag) noexcept
{
    return (flags & flag) != 0u;
}

/**
 * @brief Adds a flag to an existing flag mask.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags addFlag(
    const ParameterFlags flags,
    const ParameterFlag flag) noexcept
{
    return flags | flag;
}

/**
 * @brief Removes a flag from an existing flag mask.
 */
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags removeFlag(
    const ParameterFlags flags,
    const ParameterFlag flag) noexcept
{
    return flags & ~toMask(flag);
}

namespace detail
{
template<typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T clamp(
    const T value,
    const T minimum,
    const T maximum) noexcept
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

template<typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T clampNormalized(
    const T value) noexcept
{
    return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template<typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isFinite(
    const T value) noexcept
{
    return value == value
        && value <= std::numeric_limits<T>::max()
        && value >= -std::numeric_limits<T>::max();
}

template<typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isPositive(
    const T value) noexcept
{
    return isFinite(value) && value > static_cast<T>(0);
}
} // namespace detail

/**
 * @brief Numeric range and shaping metadata for one parameter.
 *
 * The range is deliberately value-only and contains no owner pointers, heap
 * storage, automation data, or smoothing state. The step field is used only when
 * quantization is requested. The exponent field is used by Exponential scale and
 * must be positive. Percentage parameters are expected to use a real range of
 * 0..1, while decibel parameters store real values in dB.
 */
template<typename T = f32>
struct ParameterRange
{
    static_assert(
        std::is_floating_point_v<T>,
        "ParameterRange requires a floating point type");

    using value_type = T;

    T minimum = static_cast<T>(0);      ///< Minimum real value.
    T maximum = static_cast<T>(1);      ///< Maximum real value.
    T defaultValue = static_cast<T>(0); ///< Default real value.
    T step = static_cast<T>(0);         ///< Quantization step; zero disables it.
    T exponent = static_cast<T>(1);     ///< Exponential normalized exponent.

    /**
     * @brief Returns true when the numeric range is usable.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isValid() const noexcept
    {
        return detail::isFinite(minimum)
            && detail::isFinite(maximum)
            && detail::isFinite(defaultValue)
            && detail::isFinite(step)
            && detail::isFinite(exponent)
            && maximum > minimum
            && defaultValue >= minimum
            && defaultValue <= maximum
            && step >= static_cast<T>(0)
            && exponent > static_cast<T>(0);
    }

    /**
     * @brief Returns true when the range can be used for logarithmic mapping.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isValidForLogarithmic() const noexcept
    {
        return isValid()
            && detail::isPositive(minimum)
            && detail::isPositive(maximum)
            && maximum > minimum;
    }

    /**
     * @brief Clamps a real value to this range.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T clampReal(
        const T value) const noexcept
    {
        return detail::clamp(value, minimum, maximum);
    }

    /**
     * @brief Returns true when the range has a positive quantization step.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool hasStep() const noexcept
    {
        return step > static_cast<T>(0);
    }
};

/**
 * @brief One stable entry for an enumerated parameter.
 *
 * Labels are non-owning and must point to storage that outlives the descriptor.
 * This keeps descriptors allocation-free and suitable for static tables. Enum
 * defaults and real values use the stable index rather than the entry-table
 * position.
 */
struct ParameterEnumEntry
{
    u32 index = 0;              ///< Stable option index.
    const char* label = nullptr; ///< Non-owning display label.
};

/**
 * @brief Immutable neutral descriptor for one CV_DSP parameter.
 *
 * ParameterDescriptor owns no dynamic memory and stores only scalar metadata plus
 * non-owning pointers to static text and enum-entry storage. Runtime state,
 * smoothing, automation queues, preset serialization, and framework-specific IDs
 * belong to later Manager or Adapter layers.
 */
template<typename T = f32>
class ParameterDescriptor
{
    static_assert(
        std::is_floating_point_v<T>,
        "ParameterDescriptor requires a floating point type");

public:
    using value_type = T;
    using range_type = ParameterRange<T>;

    /**
     * @brief Constructs an invalid empty descriptor.
     */
    constexpr ParameterDescriptor() noexcept = default;

    /**
     * @brief Constructs a descriptor from neutral metadata.
     */
    constexpr ParameterDescriptor(
        const ParameterID id,
        const char* shortName,
        const char* longName,
        const ParameterUnit unit,
        const ParameterScale scale,
        const ParameterFlags flags,
        const range_type range,
        const ParameterEnumEntry* enumEntries = nullptr,
        const std::size_t enumEntryCount = 0,
        const char* stableTextID = nullptr,
        const char* unitLabel = nullptr,
        const char* groupName = nullptr,
        const u32 displayPrecision = 2) noexcept
        : id_(id),
          shortName_(shortName),
          longName_(longName),
          stableTextID_(stableTextID),
          unitLabel_(unitLabel),
          groupName_(groupName),
          unit_(unit),
          scale_(scale),
          flags_(normalizeFlags(scale, flags)),
          range_(range),
          enumEntries_(enumEntries),
          enumEntryCount_(enumEntryCount),
          displayPrecision_(displayPrecision)
    {
    }

    /**
     * @brief Constructs a descriptor from neutral metadata with one flag.
     */
    constexpr ParameterDescriptor(
        const ParameterID id,
        const char* shortName,
        const char* longName,
        const ParameterUnit unit,
        const ParameterScale scale,
        const ParameterFlag flag,
        const range_type range,
        const ParameterEnumEntry* enumEntries = nullptr,
        const std::size_t enumEntryCount = 0,
        const char* stableTextID = nullptr,
        const char* unitLabel = nullptr,
        const char* groupName = nullptr,
        const u32 displayPrecision = 2) noexcept
        : ParameterDescriptor(
              id,
              shortName,
              longName,
              unit,
              scale,
              toMask(flag),
              range,
              enumEntries,
              enumEntryCount,
              stableTextID,
              unitLabel,
              groupName,
              displayPrecision)
    {
    }

    /**
     * @brief Returns the stable neutral parameter ID.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterID getID() const noexcept
    {
        return id_;
    }

    /**
     * @brief Returns the non-owning short name pointer.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const char* getShortName() const noexcept
    {
        return shortName_;
    }

    /**
     * @brief Returns the non-owning long name pointer.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const char* getLongName() const noexcept
    {
        return longName_;
    }

    /**
     * @brief Returns the optional stable textual ID used by text-oriented hosts.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const char* getStableTextID() const noexcept
    {
        return stableTextID_;
    }

    /**
     * @brief Returns the optional display unit label.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const char* getUnitLabel() const noexcept
    {
        return unitLabel_;
    }

    /**
     * @brief Returns the optional group/category name.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const char* getGroupName() const noexcept
    {
        return groupName_;
    }

    /**
     * @brief Returns the suggested decimal display precision.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr u32 getDisplayPrecision() const noexcept
    {
        return displayPrecision_;
    }

    /**
     * @brief Returns the unit metadata hint.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterUnit getUnit() const noexcept
    {
        return unit_;
    }

    /**
     * @brief Returns the normalized-value conversion scale.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterScale getScale() const noexcept
    {
        return scale_;
    }

    /**
     * @brief Returns the raw flag mask.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterFlags getFlags() const noexcept
    {
        return flags_;
    }

    /**
     * @brief Returns true when this descriptor contains the requested flag.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool hasFlag(
        const ParameterFlag flag) const noexcept
    {
        return manager::hasFlag(flags_, flag);
    }

    /**
     * @brief Returns the numeric range metadata.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const range_type& getRange() const noexcept
    {
        return range_;
    }

    /**
     * @brief Returns the stable default real value clamped to the descriptor range.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T getDefaultValue() const noexcept
    {
        return range_.clampReal(range_.defaultValue);
    }

    /**
     * @brief Returns the enum-entry table pointer.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const ParameterEnumEntry* getEnumEntries() const noexcept
    {
        return enumEntries_;
    }

    /**
     * @brief Returns the number of enum entries.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr std::size_t getEnumEntryCount() const noexcept
    {
        return enumEntryCount_;
    }

    /**
     * @brief Returns the enum entry at an index, or nullptr when unavailable.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr const ParameterEnumEntry* getEnumEntry(
        const std::size_t index) const noexcept
    {
        return enumEntries_ != nullptr && index < enumEntryCount_
            ? enumEntries_ + index
            : nullptr;
    }

    /**
     * @brief Returns the enum entry with a stable index, or nullptr when unavailable.
     */
    CVDSP_NODISCARD inline const ParameterEnumEntry* getEnumEntryByStableIndex(
        const u32 stableIndex) const noexcept
    {
        if (enumEntries_ == nullptr)
            return nullptr;

        for (std::size_t i = 0; i < enumEntryCount_; ++i)
        {
            if (enumEntries_[i].index == stableIndex)
                return enumEntries_ + i;
        }

        return nullptr;
    }

    /**
     * @brief Converts a normalized value to a stable enum index.
     */
    CVDSP_NODISCARD inline u32 normalizedToEnumIndex(
        const T normalized) const noexcept
    {
        return enumIndexAtNormalizedPosition(clampNormalized(normalized));
    }

    /**
     * @brief Converts a stable enum index to its normalized position.
     */
    CVDSP_NODISCARD inline T enumIndexToNormalized(
        const u32 stableIndex) const noexcept
    {
        if (enumEntryCount_ <= 1)
            return static_cast<T>(0);

        const std::size_t nearestPosition = findNearestEnumPosition(stableIndex);
        return clampNormalized(
            static_cast<T>(nearestPosition)
            /
            static_cast<T>(enumEntryCount_ - 1u));
    }

    /**
     * @brief Quantizes a real enum-like value to the nearest stable enum index.
     */
    CVDSP_NODISCARD inline u32 nearestEnumIndex(
        const T value) const noexcept
    {
        const std::size_t nearestPosition = findNearestEnumPosition(value);
        return enumIndexAtPosition(nearestPosition);
    }

    /**
     * @brief Returns the high-level value kind implied by scale and flags.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr ParameterValueKind getValueKind() const noexcept
    {
        if (scale_ == ParameterScale::Boolean || hasFlag(ParameterFlag::Boolean))
            return ParameterValueKind::Boolean;

        if (scale_ == ParameterScale::Enum || hasFlag(ParameterFlag::Enum))
            return ParameterValueKind::Enum;

        if (hasFlag(ParameterFlag::Discrete) || range_.hasStep())
            return ParameterValueKind::Discrete;

        return ParameterValueKind::Continuous;
    }

    /**
     * @brief Returns true when descriptor metadata is internally consistent.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isValid() const noexcept
    {
        if (shortName_ == nullptr || longName_ == nullptr)
            return false;

        if (!range_.isValid())
            return false;

        if (!flagsAreValid())
            return false;

        if (!scaleAndFlagsAreConsistent())
            return false;

        if (scale_ == ParameterScale::Logarithmic && !range_.isValidForLogarithmic())
            return false;

        if (scale_ == ParameterScale::Percentage && !rangeIsZeroToOne())
            return false;

        if (scale_ == ParameterScale::Boolean && !booleanRangeIsValid())
            return false;

        if (scale_ == ParameterScale::Enum && enumEntryCount_ == 0)
            return false;

        if (enumEntryCount_ > 0 && enumEntries_ == nullptr)
            return false;

        if (scale_ == ParameterScale::Enum && !enumEntriesAreValid())
            return false;

        return true;
    }

    /**
     * @brief Clamps a normalized value to 0..1.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T clampNormalized(
        const T normalized) const noexcept
    {
        return detail::clampNormalized(normalized);
    }

    /**
     * @brief Clamps a real value to the descriptor range.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T clampReal(
        const T value) const noexcept
    {
        return range_.clampReal(value);
    }

    /**
     * @brief Quantizes a real value according to boolean, enum, or step metadata.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T quantizeReal(
        const T value) const noexcept
    {
        if (getValueKind() == ParameterValueKind::Enum)
            return static_cast<T>(nearestEnumIndex(value));

        const T clamped = clampReal(value);

        if (getValueKind() == ParameterValueKind::Boolean)
            return clamped >= midpoint() ? static_cast<T>(1) : static_cast<T>(0);

        if (range_.hasStep())
        {
            const T steps = std::round((clamped - range_.minimum) / range_.step);
            return clampReal(range_.minimum + steps * range_.step);
        }

        return clamped;
    }

    /**
     * @brief Converts a normalized 0..1 value into the descriptor's real value.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T normalizedToReal(
        const T normalized) const noexcept
    {
        const T n = clampNormalized(normalized);

        switch (scale_)
        {
            case ParameterScale::Boolean:
                return n >= static_cast<T>(0.5) ? static_cast<T>(1) : static_cast<T>(0);

            case ParameterScale::Enum:
                return static_cast<T>(normalizedToEnumIndex(n));

            case ParameterScale::Logarithmic:
                return quantizeReal(normalizedToLogarithmic(n));

            case ParameterScale::Exponential:
                return quantizeReal(normalizedToExponential(n));

            case ParameterScale::Decibel:
            case ParameterScale::Percentage:
            case ParameterScale::Linear:
            default:
                return quantizeReal(linearMap(n));
        }
    }

    /**
     * @brief Converts a real descriptor value into normalized 0..1 form.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T realToNormalized(
        const T realValue) const noexcept
    {
        const T value = quantizeReal(realValue);

        switch (scale_)
        {
            case ParameterScale::Boolean:
                return value >= static_cast<T>(0.5) ? static_cast<T>(1) : static_cast<T>(0);

            case ParameterScale::Enum:
                return enumIndexToNormalized(nearestEnumIndex(value));

            case ParameterScale::Logarithmic:
                return logarithmicToNormalized(value);

            case ParameterScale::Exponential:
                return exponentialToNormalized(value);

            case ParameterScale::Decibel:
            case ParameterScale::Percentage:
            case ParameterScale::Linear:
            default:
                return linearToNormalized(value);
        }
    }

    /**
     * @brief Returns true when host adapters may automate this parameter.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isAutomatable() const noexcept
    {
        return hasFlag(ParameterFlag::Automatable) && !hasFlag(ParameterFlag::ReadOnly);
    }

    /**
     * @brief Returns true when modulation systems may target this parameter.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isModulatable() const noexcept
    {
        return hasFlag(ParameterFlag::Modulatable) && !hasFlag(ParameterFlag::ReadOnly);
    }

    /**
     * @brief Returns true when preset/state systems should persist this parameter.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool isPersistent() const noexcept
    {
        return hasFlag(ParameterFlag::Persistent);
    }

private:
    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool flagsAreValid() const noexcept
    {
        return (flags_ & ~ValidParameterFlags) == 0u;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool scaleAndFlagsAreConsistent() const noexcept
    {
        if (scale_ != ParameterScale::Boolean && hasFlag(ParameterFlag::Boolean))
            return false;

        if (scale_ != ParameterScale::Enum && hasFlag(ParameterFlag::Enum))
            return false;

        if (scale_ == ParameterScale::Boolean && hasFlag(ParameterFlag::Enum))
            return false;

        if (scale_ == ParameterScale::Enum && hasFlag(ParameterFlag::Boolean))
            return false;

        if (hasFlag(ParameterFlag::Bypass) && scale_ != ParameterScale::Boolean)
            return false;

        return true;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool rangeIsZeroToOne() const noexcept
    {
        return range_.minimum == static_cast<T>(0)
            && range_.maximum == static_cast<T>(1);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool booleanRangeIsValid() const noexcept
    {
        return rangeIsZeroToOne()
            && (range_.defaultValue == static_cast<T>(0)
                || range_.defaultValue == static_cast<T>(1));
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr ParameterFlags normalizeFlags(
        const ParameterScale scale,
        const ParameterFlags flags) noexcept
    {
        ParameterFlags result = flags;

        if (scale == ParameterScale::Boolean)
            result = addFlag(addFlag(result, ParameterFlag::Boolean), ParameterFlag::Discrete);

        if (scale == ParameterScale::Enum)
            result = addFlag(addFlag(result, ParameterFlag::Enum), ParameterFlag::Discrete);

        return result;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T midpoint() const noexcept
    {
        return (range_.minimum + range_.maximum) * static_cast<T>(0.5);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T linearMap(
        const T normalized) const noexcept
    {
        return range_.minimum + (range_.maximum - range_.minimum) * normalized;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T linearToNormalized(
        const T value) const noexcept
    {
        const T width = range_.maximum - range_.minimum;
        if (width <= static_cast<T>(0))
            return static_cast<T>(0);

        return clampNormalized((value - range_.minimum) / width);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T normalizedToLogarithmic(
        const T normalized) const noexcept
    {
        if (!range_.isValidForLogarithmic())
            return linearMap(normalized);

        const T minLog = std::log(range_.minimum);
        const T maxLog = std::log(range_.maximum);
        return std::exp(minLog + (maxLog - minLog) * normalized);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T logarithmicToNormalized(
        const T value) const noexcept
    {
        if (!range_.isValidForLogarithmic())
            return linearToNormalized(value);

        const T clamped = clampReal(value);
        const T minLog = std::log(range_.minimum);
        const T maxLog = std::log(range_.maximum);
        const T width = maxLog - minLog;

        if (width <= static_cast<T>(0))
            return static_cast<T>(0);

        return clampNormalized((std::log(clamped) - minLog) / width);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T normalizedToExponential(
        const T normalized) const noexcept
    {
        const T exponent = range_.exponent > static_cast<T>(0)
            ? range_.exponent
            : static_cast<T>(1);

        return linearMap(std::pow(normalized, exponent));
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T exponentialToNormalized(
        const T value) const noexcept
    {
        const T exponent = range_.exponent > static_cast<T>(0)
            ? range_.exponent
            : static_cast<T>(1);

        const T linear = linearToNormalized(value);
        return clampNormalized(std::pow(linear, static_cast<T>(1) / exponent));
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr bool enumEntriesAreValid() const noexcept
    {
        if (enumEntryCount_ == 0)
            return true;

        if (enumEntries_ == nullptr)
            return false;

        bool defaultMatchesEntry = false;

        for (std::size_t i = 0; i < enumEntryCount_; ++i)
        {
            if (enumEntries_[i].label == nullptr)
                return false;

            const T index = static_cast<T>(enumEntries_[i].index);

            if (index < range_.minimum || index > range_.maximum)
                return false;

            if (index == range_.defaultValue)
                defaultMatchesEntry = true;
        }

        return defaultMatchesEntry;
    }

    CVDSP_NODISCARD inline u32 enumIndexAtNormalizedPosition(
        const T normalized) const noexcept
    {
        if (enumEntryCount_ == 0)
            return 0;

        const std::size_t maxPosition = enumEntryCount_ - 1u;
        const std::size_t position = static_cast<std::size_t>(
            detail::clamp(
                std::round(normalized * static_cast<T>(maxPosition)),
                static_cast<T>(0),
                static_cast<T>(maxPosition)));

        return enumIndexAtPosition(position);
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE u32 enumIndexAtPosition(
        const std::size_t position) const noexcept
    {
        if (enumEntryCount_ == 0)
            return 0;

        const std::size_t clampedPosition = position < enumEntryCount_
            ? position
            : enumEntryCount_ - 1u;

        return enumEntries_ != nullptr
            ? enumEntries_[clampedPosition].index
            : static_cast<u32>(clampedPosition);
    }

    CVDSP_NODISCARD inline std::size_t findNearestEnumPosition(
        const u32 stableIndex) const noexcept
    {
        if (enumEntryCount_ == 0)
            return 0;

        if (enumEntries_ == nullptr)
            return stableIndex < enumEntryCount_
                ? static_cast<std::size_t>(stableIndex)
                : enumEntryCount_ - 1u;

        std::size_t nearestPosition = 0;
        u32 nearestDistance = stableIndex >= enumEntries_[0].index
            ? stableIndex - enumEntries_[0].index
            : enumEntries_[0].index - stableIndex;

        for (std::size_t i = 1; i < enumEntryCount_; ++i)
        {
            const u32 distance = stableIndex >= enumEntries_[i].index
                ? stableIndex - enumEntries_[i].index
                : enumEntries_[i].index - stableIndex;

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestPosition = i;
            }
        }

        return nearestPosition;
    }

    CVDSP_NODISCARD inline std::size_t findNearestEnumPosition(
        const T value) const noexcept
    {
        if (enumEntryCount_ == 0)
            return 0;

        if (enumEntries_ == nullptr)
        {
            const T maxPosition = static_cast<T>(enumEntryCount_ - 1u);
            return static_cast<std::size_t>(
                detail::clamp(std::round(value), static_cast<T>(0), maxPosition));
        }

        std::size_t nearestPosition = 0;
        T nearestDistance = absoluteDifference(
            value,
            static_cast<T>(enumEntries_[0].index));

        for (std::size_t i = 1; i < enumEntryCount_; ++i)
        {
            const T distance = absoluteDifference(
                value,
                static_cast<T>(enumEntries_[i].index));

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestPosition = i;
            }
        }

        return nearestPosition;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE static constexpr T absoluteDifference(
        const T lhs,
        const T rhs) noexcept
    {
        return lhs >= rhs ? lhs - rhs : rhs - lhs;
    }

private:
    ParameterID id_ = 0;
    const char* shortName_ = nullptr;
    const char* longName_ = nullptr;
    const char* stableTextID_ = nullptr;
    const char* unitLabel_ = nullptr;
    const char* groupName_ = nullptr;
    ParameterUnit unit_ = ParameterUnit::None;
    ParameterScale scale_ = ParameterScale::Linear;
    ParameterFlags flags_ = toMask(ParameterFlag::None);
    range_type range_{};
    const ParameterEnumEntry* enumEntries_ = nullptr;
    std::size_t enumEntryCount_ = 0;
    u32 displayPrecision_ = 2;
};

} // namespace cvdsp::manager

#endif // CVDSP_MANAGER_PARAMETERDESCRIPTOR_HPP
