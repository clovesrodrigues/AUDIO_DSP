#ifndef CVDSP_ADAPTERS_VST2_CONFIG_HPP
#define CVDSP_ADAPTERS_VST2_CONFIG_HPP

/**
 * @file VST2Config.hpp
 * @brief Common VST2 adapter configuration for internal/local Windows builds.
 *
 * The VST2 adapter is intentionally small and host-facing. It is meant to be
 * used by local/internal wrappers while the DSP remains independent from the
 * Steinberg SDK.
 */

#include "public.sdk/source/vst2.x/audioeffectx.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cvdsp::adapters::vst2
{

using ParameterIndex = VstInt32;
using NormalizedValue = float;

inline constexpr std::uint32_t kStateChunkMagic = 0x43563253u; // "CV2S"
inline constexpr std::uint32_t kStateChunkVersion = 1u;
inline constexpr std::size_t kDefaultMaxParameters = 128u;

inline void copyString(char* destination, const std::size_t capacity, const char* source) noexcept
{
    if (destination == nullptr || capacity == 0)
        return;

    std::size_t index = 0;
    if (source != nullptr)
    {
        for (; index + 1 < capacity && source[index] != '\0'; ++index)
            destination[index] = source[index];
    }
    destination[index] = '\0';
}

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_CONFIG_HPP
