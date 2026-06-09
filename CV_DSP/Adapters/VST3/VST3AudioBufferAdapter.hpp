#ifndef CVDSP_ADAPTERS_VST3_VST3AUDIOBUFFERADAPTER_HPP
#define CVDSP_ADAPTERS_VST3_VST3AUDIOBUFFERADAPTER_HPP

/**
 * @file VST3AudioBufferAdapter.hpp
 * @brief Stateless zero-copy VST3 audio-buffer adapter for CV_DSP.
 *
 * Header-only
 * C++17-compatible
 * Real-time safe
 *
 * This adapter translates Steinberg VST3 AudioBusBuffers into CV_DSP
 * AudioBufferView<T> and ConstAudioBufferView<T> objects. It performs no
 * allocation, owns no memory, copies no samples, throws no exceptions, and uses
 * no RTTI. Returned views are non-owning and must not outlive the VST3 process
 * call that supplied the source AudioBusBuffers.
 *
 * The adapter does not process audio, adapt process context, adapt parameters,
 * inspect MIDI/events, or manage presets/state. It only validates VST3 bus
 * buffers and exposes them through the neutral CV_DSP buffer-view types.
 */

#include "../../Core/AudioBufferView.hpp"

#include <cstddef>

#include <pluginterfaces/vst/ivstaudioprocessor.h>

namespace cvdsp::adapters::vst3
{

/**
 * @brief Converts Steinberg::Vst::AudioBusBuffers to CV_DSP audio views.
 *
 * All functions are static and stateless. A returned empty/default view means
 * the requested bus cannot be used for audio processing in the current block,
 * for example because the bus is inactive, numSamples is zero, the channel
 * array is null, or one or more channel pointers are null.
 */
class VST3AudioBufferAdapter final
{
public:
    VST3AudioBufferAdapter() = delete;
    VST3AudioBufferAdapter(const VST3AudioBufferAdapter&) = delete;
    VST3AudioBufferAdapter& operator=(const VST3AudioBufferAdapter&) = delete;
    ~VST3AudioBufferAdapter() = delete;

    /**
     * @brief Creates a mutable 32-bit CV_DSP view over VST3 channelBuffers32.
     */
    [[nodiscard]] static cvdsp::AudioBufferView<float> makeMutable32(
        Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        if (!isValid32(bus, numSamples))
            return {};

        return cvdsp::AudioBufferView<float>(
            bus.channelBuffers32,
            channelCount(bus),
            numSamples);
    }

    /**
     * @brief Creates a mutable 64-bit CV_DSP view over VST3 channelBuffers64.
     */
    [[nodiscard]] static cvdsp::AudioBufferView<double> makeMutable64(
        Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        if (!isValid64(bus, numSamples))
            return {};

        return cvdsp::AudioBufferView<double>(
            bus.channelBuffers64,
            channelCount(bus),
            numSamples);
    }

    /**
     * @brief Creates a const 32-bit CV_DSP view over VST3 channelBuffers32.
     */
    [[nodiscard]] static cvdsp::ConstAudioBufferView<float> makeConst32(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        if (!isValid32(bus, numSamples))
            return {};

        return cvdsp::ConstAudioBufferView<float>(
            bus.channelBuffers32,
            channelCount(bus),
            numSamples);
    }

    /**
     * @brief Creates a const 64-bit CV_DSP view over VST3 channelBuffers64.
     */
    [[nodiscard]] static cvdsp::ConstAudioBufferView<double> makeConst64(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        if (!isValid64(bus, numSamples))
            return {};

        return cvdsp::ConstAudioBufferView<double>(
            bus.channelBuffers64,
            channelCount(bus),
            numSamples);
    }

    /**
     * @brief Converts a VST3 sample count to std::size_t, rejecting negatives.
     */
    [[nodiscard]] static constexpr std::size_t sampleCount(
        const Steinberg::int32 numSamples) noexcept
    {
        return numSamples > 0
            ? static_cast<std::size_t>(numSamples)
            : static_cast<std::size_t>(0);
    }

    /**
     * @brief Converts a VST3 bus channel count to std::size_t, rejecting negatives.
     */
    [[nodiscard]] static constexpr std::size_t channelCount(
        const Steinberg::Vst::AudioBusBuffers& bus) noexcept
    {
        return bus.numChannels > 0
            ? static_cast<std::size_t>(bus.numChannels)
            : static_cast<std::size_t>(0);
    }

    /**
     * @brief Returns true when a bus has at least one channel.
     */
    [[nodiscard]] static constexpr bool hasChannels(
        const Steinberg::Vst::AudioBusBuffers& bus) noexcept
    {
        return channelCount(bus) > 0;
    }

    /**
     * @brief Returns true when a 32-bit bus can produce a usable CV_DSP view.
     */
    [[nodiscard]] static bool isValid32(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        return numSamples > 0
            && hasChannels(bus)
            && bus.channelBuffers32 != nullptr
            && allChannelPointersValid(bus.channelBuffers32, channelCount(bus));
    }

    /**
     * @brief Returns true when a 64-bit bus can produce a usable CV_DSP view.
     */
    [[nodiscard]] static bool isValid64(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        return numSamples > 0
            && hasChannels(bus)
            && bus.channelBuffers64 != nullptr
            && allChannelPointersValid(bus.channelBuffers64, channelCount(bus));
    }

    /**
     * @brief Returns true when a 32-bit bus can produce a usable CV_DSP view.
     */
    [[nodiscard]] static bool isValid32(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const Steinberg::int32 numSamples) noexcept
    {
        return isValid32(bus, sampleCount(numSamples));
    }

    /**
     * @brief Returns true when a 64-bit bus can produce a usable CV_DSP view.
     */
    [[nodiscard]] static bool isValid64(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const Steinberg::int32 numSamples) noexcept
    {
        return isValid64(bus, sampleCount(numSamples));
    }

    /**
     * @brief Returns true when the bus has no usable audio for this block.
     */
    [[nodiscard]] static constexpr bool isInactive(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t numSamples) noexcept
    {
        return numSamples == 0 || !hasChannels(bus);
    }

    /**
     * @brief Returns true when the VST3 silent bit for channel is set.
     */
    [[nodiscard]] static constexpr bool isChannelSilent(
        const Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t channel) noexcept
    {
        return channel < SilenceFlagBits
            && (bus.silenceFlags & silenceMask(channel)) != 0u;
    }

    /**
     * @brief Returns true when all representable active channels are marked silent.
     */
    [[nodiscard]] static constexpr bool isBusSilent(
        const Steinberg::Vst::AudioBusBuffers& bus) noexcept
    {
        const std::size_t channels = channelCount(bus);
        if (channels == 0)
            return true;

        const std::size_t checkedChannels = channels < SilenceFlagBits
            ? channels
            : SilenceFlagBits;

        for (std::size_t channel = 0; channel < checkedChannels; ++channel)
        {
            if (!isChannelSilent(bus, channel))
                return false;
        }

        return true;
    }

    /**
     * @brief Marks one output channel as silent in VST3 silenceFlags.
     */
    static void markChannelSilent(
        Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t channel) noexcept
    {
        if (channel < SilenceFlagBits)
            bus.silenceFlags |= silenceMask(channel);
    }

    /**
     * @brief Clears the silent state of one output channel in VST3 silenceFlags.
     */
    static void clearChannelSilent(
        Steinberg::Vst::AudioBusBuffers& bus,
        const std::size_t channel) noexcept
    {
        if (channel < SilenceFlagBits)
            bus.silenceFlags &= ~silenceMask(channel);
    }

    /**
     * @brief Marks all active, representable output channels as silent.
     */
    static void markAllChannelsSilent(
        Steinberg::Vst::AudioBusBuffers& bus) noexcept
    {
        const std::size_t channels = channelCount(bus);
        const std::size_t checkedChannels = channels < SilenceFlagBits
            ? channels
            : SilenceFlagBits;

        bus.silenceFlags = 0u;
        for (std::size_t channel = 0; channel < checkedChannels; ++channel)
            bus.silenceFlags |= silenceMask(channel);
    }

    /**
     * @brief Clears all VST3 silenceFlags for the bus.
     */
    static void clearAllSilenceFlags(
        Steinberg::Vst::AudioBusBuffers& bus) noexcept
    {
        bus.silenceFlags = 0u;
    }

    /**
     * @brief Returns the raw VST3 silence flag mask.
     */
    [[nodiscard]] static constexpr Steinberg::uint64 silenceFlags(
        const Steinberg::Vst::AudioBusBuffers& bus) noexcept
    {
        return bus.silenceFlags;
    }

private:
    static constexpr std::size_t SilenceFlagBits = 64;

    /**
     * @brief Creates a VST3 silence flag mask for one channel.
     */
    [[nodiscard]] static constexpr Steinberg::uint64 silenceMask(
        const std::size_t channel) noexcept
    {
        return channel < SilenceFlagBits
            ? (static_cast<Steinberg::uint64>(1) << channel)
            : static_cast<Steinberg::uint64>(0);
    }

    /**
     * @brief Validates every channel pointer in a channel pointer array.
     */
    template<typename T>
    [[nodiscard]] static bool allChannelPointersValid(
        T* const* channels,
        const std::size_t numChannels) noexcept
    {
        if (channels == nullptr)
            return false;

        for (std::size_t channel = 0; channel < numChannels; ++channel)
        {
            if (channels[channel] == nullptr)
                return false;
        }

        return true;
    }
};

} // namespace cvdsp::adapters::vst3

#endif // CVDSP_ADAPTERS_VST3_VST3AUDIOBUFFERADAPTER_HPP
