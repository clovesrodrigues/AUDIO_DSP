#ifndef CVDSP_CORE_AUDIOBUFFERVIEW_HPP
#define CVDSP_CORE_AUDIOBUFFERVIEW_HPP

/**
 * @file AudioBufferView.hpp
 * @brief Non-owning Audio Buffer View
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Features:
 * - Non-owning
 * - No allocation
 * - No resizing
 * - No ownership
 * - Zero-copy access
 *
 * Compatible with:
 * - VST3
 * - iPlug2
 * - JUCE
 * - CLAP
 *
 * Inspired by:
 * - std::span
 * - gsl::span
 */

#include <cassert>
#include <cstddef>
#include <type_traits>

namespace cvdsp
{

/**
 * @brief Non-owning audio buffer view.
 *
 * Layout:
 *
 * channels_[channel][sample]
 *
 * No memory ownership.
 *
 * No allocation.
 *
 * No resizing.
 */
template<typename T>
class AudioBufferView
{
    static_assert(
        std::is_floating_point_v<T>,
        "AudioBufferView requires floating point type");

public:

    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using size_type       = std::size_t;

public:

    /**
     * @brief Default constructor.
     */
    constexpr AudioBufferView() noexcept = default;

    /**
     * @brief Construct from channel array.
     *
     * channels must remain valid during
     * the entire lifetime of the view.
     */
    constexpr AudioBufferView(
        T* const* channels,
        size_type numChannels,
        size_type numSamples) noexcept
        :
        channels_(channels),
        numChannels_(numChannels),
        numSamples_(numSamples)
    {
    }

    /**
     * @brief Construct from mutable channel pointers.
     */
    template<std::size_t NumChannels>
    constexpr AudioBufferView(
        T* (&channels)[NumChannels],
        size_type numSamples) noexcept
        :
        channels_(channels),
        numChannels_(NumChannels),
        numSamples_(numSamples)
    {
    }

    /**
     * @brief Returns channel pointer.
     */
    [[nodiscard]]
    constexpr T* getChannel(
        size_type channel) const noexcept
    {
        assert(channel < numChannels_);

        return channels_[channel];
    }

    /**
     * @brief Returns number of channels.
     */
    [[nodiscard]]
    constexpr size_type getNumChannels() const noexcept
    {
        return numChannels_;
    }

    /**
     * @brief Returns number of samples.
     */
    [[nodiscard]]
    constexpr size_type getNumSamples() const noexcept
    {
        return numSamples_;
    }

    /**
     * @brief Channel access.
     *
     * Example:
     *
     * buffer[0][100]
     */
    [[nodiscard]]
    constexpr T* operator[](
        size_type channel) const noexcept
    {
        return getChannel(channel);
    }

    /**
     * @brief Check if view is valid.
     */
    [[nodiscard]]
    constexpr bool isValid() const noexcept
    {
        return
            channels_ != nullptr
            &&
            numChannels_ > 0
            &&
            numSamples_ > 0;
    }

    /**
     * @brief Check if empty.
     */
    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return
            numChannels_ == 0
            ||
            numSamples_ == 0;
    }

private:

    /**
     * Non-owning array of channel pointers.
     */
    T* const* channels_ = nullptr;

    /**
     * Channel count.
     */
    size_type numChannels_ = 0;

    /**
     * Samples per channel.
     */
    size_type numSamples_ = 0;
};



/**
 * @brief Const version.
 */
template<typename T>
class ConstAudioBufferView
{
    static_assert(
        std::is_floating_point_v<T>,
        "ConstAudioBufferView requires floating point type");

public:

    using value_type      = T;
    using pointer         = const T*;
    using size_type       = std::size_t;

public:

    constexpr ConstAudioBufferView() noexcept = default;

    constexpr ConstAudioBufferView(
        const T* const* channels,
        size_type numChannels,
        size_type numSamples) noexcept
        :
        channels_(channels),
        numChannels_(numChannels),
        numSamples_(numSamples)
    {
    }

    template<std::size_t NumChannels>
    constexpr ConstAudioBufferView(
        const T* (&channels)[NumChannels],
        size_type numSamples) noexcept
        :
        channels_(channels),
        numChannels_(NumChannels),
        numSamples_(numSamples)
    {
    }

    [[nodiscard]]
    constexpr const T* getChannel(
        size_type channel) const noexcept
    {
        assert(channel < numChannels_);

        return channels_[channel];
    }

    [[nodiscard]]
    constexpr size_type getNumChannels() const noexcept
    {
        return numChannels_;
    }

    [[nodiscard]]
    constexpr size_type getNumSamples() const noexcept
    {
        return numSamples_;
    }

    [[nodiscard]]
    constexpr const T* operator[](
        size_type channel) const noexcept
    {
        return getChannel(channel);
    }

    [[nodiscard]]
    constexpr bool isValid() const noexcept
    {
        return
            channels_ != nullptr
            &&
            numChannels_ > 0
            &&
            numSamples_ > 0;
    }

private:

    const T* const* channels_ = nullptr;

    size_type numChannels_ = 0;

    size_type numSamples_ = 0;
};

} // namespace cvdsp

#endif
