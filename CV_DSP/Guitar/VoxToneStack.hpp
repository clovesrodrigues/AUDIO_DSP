#ifndef CVDSP_GUITAR_VOXTONESTACK_HPP
#define CVDSP_GUITAR_VOXTONESTACK_HPP

/**
 * @file VoxToneStack.hpp
 * @brief Vox AC15 / AC30 Top Boost Tone Stack
 *
 * Inspired by:
 *
 * - Vox AC15
 * - Vox AC30
 * - Top Boost Channel
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - ToneStack.hpp
 *
 * No allocations.
 * No exceptions.
 */

#include <algorithm>
#include <cmath>

#include "ToneStack.hpp"

namespace cvdsp
{

template<typename T>
class VoxToneStack final
    : public ToneStack<T>
{
public:

    VoxToneStack() = default;

    void prepare(
        T sampleRate)
        noexcept override
    {
        ToneStack<T>::prepare(
            sampleRate);

        updateFilters();
    }

    void reset()
        noexcept override
    {
        ToneStack<T>::reset();
    }

    void setBass(
        T value)
        noexcept override
    {
        this->bass_ =
            std::clamp(
                value,
                T(0),
                T(1));

        updateFilters();
    }

    void setMid(
        T value)
        noexcept override
    {
        this->mid_ =
            std::clamp(
                value,
                T(0),
                T(1));

        updateFilters();
    }

    void setTreble(
        T value)
        noexcept override
    {
        this->treble_ =
            std::clamp(
                value,
                T(0),
                T(1));

        updateFilters();
    }

    void setPresence(
        T value)
        noexcept override
    {
        this->presence_ =
            std::clamp(
                value,
                T(0),
                T(1));

        updateFilters();
    }

    T process(
        T input)
        noexcept override
    {
        T x = input;

        /**
         * Vox Top Boost topology.
         *
         * Bass
         * ->
         * Mid shaping
         * ->
         * Treble
         * ->
         * Presence
         */

        x =
            this->bassFilter_
                .process(x);

        x =
            this->midFilter_
                .process(x);

        x =
            this->trebleFilter_
                .process(x);

        x =
            this->presenceFilter_
                .process(x);

        return x;
    }

private:

    void updateFilters()
        noexcept
    {
        /**
         * AC15 / AC30 Top Boost
         *
         * Character:
         *
         * - Chime
         * - Sparkle
         * - Upper-mid emphasis
         * - Bright top end
         * - Less low-end than Fender
         */

        const T bassGainDb =
            static_cast<T>(-10)
            +
            (
                this->bass_
                *
                static_cast<T>(20)
            );

        const T midGainDb =
            static_cast<T>(-6)
            +
            (
                this->mid_
                *
                static_cast<T>(12)
            );

        const T trebleGainDb =
            static_cast<T>(-12)
            +
            (
                this->treble_
                *
                static_cast<T>(24)
            );

        const T presenceGainDb =
            this->presence_
            *
            static_cast<T>(10);

        /**
         * Tight bass response.
         */

        this->bassFilter_
            .setLowShelf(
                this->sampleRate_,
                static_cast<T>(100.0),
                static_cast<T>(0.707),
                bassGainDb);

        /**
         * Upper-mid chime region.
         */

        this->midFilter_
            .setPeak(
                this->sampleRate_,
                static_cast<T>(1200.0),
                static_cast<T>(0.8),
                midGainDb);

        /**
         * Top Boost treble section.
         */

        this->trebleFilter_
            .setHighShelf(
                this->sampleRate_,
                static_cast<T>(3500.0),
                static_cast<T>(0.707),
                trebleGainDb);

        /**
         * Air and brilliance.
         */

        this->presenceFilter_
            .setHighShelf(
                this->sampleRate_,
                static_cast<T>(7000.0),
                static_cast<T>(0.707),
                presenceGainDb);
    }
};

using VoxToneStackF =
    VoxToneStack<float>;

using VoxToneStackD =
    VoxToneStack<double>;

} // namespace cvdsp

#endif
