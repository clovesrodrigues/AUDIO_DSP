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
            ToneStack<T>::clampControl(
                value);

        updateFilters();
    }

    void setMid(
        T value)
        noexcept override
    {
        this->mid_ =
            ToneStack<T>::clampControl(
                value);

        updateFilters();
    }

    void setTreble(
        T value)
        noexcept override
    {
        this->treble_ =
            ToneStack<T>::clampControl(
                value);

        updateFilters();
    }

    void setPresence(
        T value)
        noexcept override
    {
        this->presence_ =
            ToneStack<T>::clampControl(
                value);

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

        const T cutInteraction =
            this->treble_
            -
            this->bass_;

        const T bassGainDb =
            static_cast<T>(-13)
            +
            (
                this->bass_
                *
                static_cast<T>(18)
            )
            -
            (
                this->treble_
                *
                static_cast<T>(3)
            );

        const T midGainDb =
            static_cast<T>(-8)
            +
            (
                this->mid_
                *
                static_cast<T>(14)
            )
            +
            (
                cutInteraction
                *
                static_cast<T>(2)
            );

        const T trebleGainDb =
            static_cast<T>(-13)
            +
            (
                this->treble_
                *
                static_cast<T>(25)
            )
            -
            (
                this->bass_
                *
                static_cast<T>(2)
            );

        const T presenceGainDb =
            static_cast<T>(-1)
            +
            (
                this->presence_
                *
                static_cast<T>(11)
            );

        /**
         * Tight bass response.
         */

        ToneStack<T>::configureLowShelf(
            this->bassFilter_,
            static_cast<T>(100.0),
            static_cast<T>(0.707),
            bassGainDb);

        /**
         * Upper-mid chime region.
         */

        ToneStack<T>::configurePeak(
            this->midFilter_,
            static_cast<T>(1200.0),
            static_cast<T>(0.8),
            midGainDb);

        /**
         * Top Boost treble section.
         */

        ToneStack<T>::configureHighShelf(
            this->trebleFilter_,
            static_cast<T>(3500.0),
            static_cast<T>(0.707),
            trebleGainDb);

        /**
         * Air and brilliance.
         */

        ToneStack<T>::configureHighShelf(
            this->presenceFilter_,
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
