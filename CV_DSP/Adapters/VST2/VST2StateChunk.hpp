#ifndef CVDSP_ADAPTERS_VST2_STATECHUNK_HPP
#define CVDSP_ADAPTERS_VST2_STATECHUNK_HPP

/**
 * @file VST2StateChunk.hpp
 * @brief Small versioned chunk format for internal VST2 parameter snapshots.
 */

#include "VST2ParameterState.hpp"

#include <vector>

namespace cvdsp::adapters::vst2
{

struct VST2StateChunkHeader
{
    std::uint32_t magic = kStateChunkMagic;
    std::uint32_t version = kStateChunkVersion;
    std::uint32_t count = 0;
};

struct VST2StateChunkEntry
{
    std::uint32_t id = 0;
    float normalized = 0.0f;
};

template<typename T, std::size_t MaxParameters>
[[nodiscard]] std::vector<std::uint8_t> makeStateChunk(
    const VST2ParameterState<T, MaxParameters>& state)
{
    VST2StateChunkHeader header {};
    header.count = static_cast<std::uint32_t>(state.size());

    std::vector<std::uint8_t> bytes(
        sizeof(VST2StateChunkHeader) + sizeof(VST2StateChunkEntry) * state.size());

    std::memcpy(bytes.data(), &header, sizeof(header));

    for (std::size_t index = 0; index < state.size(); ++index)
    {
        const auto* info = state.getInfoByIndex(index);
        VST2StateChunkEntry entry {};
        entry.id = info != nullptr ? info->id : 0u;
        entry.normalized = state.getNormalizedByIndex(index);

        std::memcpy(
            bytes.data() + sizeof(header) + sizeof(VST2StateChunkEntry) * index,
            &entry,
            sizeof(entry));
    }

    return bytes;
}

template<typename T, std::size_t MaxParameters, typename Callback>
[[nodiscard]] bool restoreStateChunk(
    VST2ParameterState<T, MaxParameters>& state,
    const void* data,
    const VstInt32 byteSize,
    Callback&& onParameterRestored) noexcept
{
    if (data == nullptr || byteSize < static_cast<VstInt32>(sizeof(VST2StateChunkHeader)))
        return false;

    VST2StateChunkHeader header {};
    std::memcpy(&header, data, sizeof(header));
    if (header.magic != kStateChunkMagic || header.version != kStateChunkVersion)
        return false;

    if (header.count > MaxParameters)
        return false;

    const std::size_t requiredSize =
        sizeof(VST2StateChunkHeader) + sizeof(VST2StateChunkEntry) * static_cast<std::size_t>(header.count);
    if (byteSize < static_cast<VstInt32>(requiredSize))
        return false;

    for (std::uint32_t entryIndex = 0; entryIndex < header.count; ++entryIndex)
    {
        VST2StateChunkEntry entry {};
        std::memcpy(
            &entry,
            static_cast<const std::uint8_t*>(data) + sizeof(VST2StateChunkHeader) + sizeof(VST2StateChunkEntry) * entryIndex,
            sizeof(entry));

        const manager::ParameterID id = entry.id;
        const NormalizedValue normalized = std::clamp(entry.normalized, 0.0f, 1.0f);
        if (state.setNormalizedByID(id, normalized))
            onParameterRestored(id, normalized);
    }

    return true;
}

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_STATECHUNK_HPP
