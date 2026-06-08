#ifndef CVDSP_GUITAR_MARSHALLTONESTACK_HPP
#define CVDSP_GUITAR_MARSHALLTONESTACK_HPP

/**
 * @file MarshallToneStack.hpp
 * @brief Marshall Tone Stack
 *
 * Inspired by:
 *
 * - Plexi 1959
 * - JMP
 * - JCM800
 * - JCM900
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
class MarshallToneStack final
    : public ToneStack<T>
{
public:

    MarshallToneStack() = default;

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
         * Marshall topology.
         *
         * Bass
         * →
         * Mid
         * →
         * Treble
         * →
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
         * Marshall stacks generally
         * exhibit:
         *
         * - tighter bass
         * - stronger mids
         * - aggressive upper mids
         * - bright presence region
         */

        const T bassInteraction =
            this->bass_
            -
            static_cast<T>(0.5);

        const T trebleInteraction =
            this->treble_
            -
            static_cast<T>(0.5);

        const T bassGainDb =
            static_cast<T>(-12)
            +
            (
                this->bass_
                *
                static_cast<T>(20)
            )
            -
            (
                trebleInteraction
                *
                static_cast<T>(2)
            );

        const T midGainDb =
            static_cast<T>(-12)
            +
            (
                this->mid_
                *
                static_cast<T>(28)
            )
            +
            (
                trebleInteraction
                *
                static_cast<T>(2)
            )
            -
            (
                bassInteraction
                *
                static_cast<T>(1.5)
            );

        const T trebleGainDb =
            static_cast<T>(-11)
            +
            (
                this->treble_
                *
                static_cast<T>(22)
            );

        const T presenceGainDb =
            static_cast<T>(-1)
            +
            (
                this->presence_
                *
                static_cast<T>(13)
            );

        /**
         * Bass
         *
         * Marshall low-end is tighter
         * than Fender.
         */

        ToneStack<T>::configureLowShelf(
            this->bassFilter_,
            static_cast<T>(120.0),
            static_cast<T>(0.707),
            bassGainDb);

        /**
         * Midrange emphasis.
         *
         * Classic Marshall growl.
         */

        ToneStack<T>::configurePeak(
            this->midFilter_,
            static_cast<T>(700.0),
            static_cast<T>(0.8),
            midGainDb);

        /**
         * Upper-mid / treble attack.
         */

        ToneStack<T>::configureHighShelf(
            this->trebleFilter_,
            static_cast<T>(2500.0),
            static_cast<T>(0.707),
            trebleGainDb);

        /**
         * Presence region.
         *
         * Power amp feedback style
         * brightness control.
         */

        ToneStack<T>::configureHighShelf(
            this->presenceFilter_,
            static_cast<T>(4500.0),
            static_cast<T>(0.707),
            presenceGainDb);
    }
};

using MarshallToneStackF =
    MarshallToneStack<float>;

using MarshallToneStackD =
    MarshallToneStack<double>;

} // namespace cvdsp

#endif
