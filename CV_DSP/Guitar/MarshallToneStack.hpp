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

        const T bassGainDb =
            static_cast<T>(-10)
            +
            (
                this->bass_
                *
                static_cast<T>(20)
            );

        const T midGainDb =
            static_cast<T>(-8)
            +
            (
                this->mid_
                *
                static_cast<T>(24)
            );

        const T trebleGainDb =
            static_cast<T>(-10)
            +
            (
                this->treble_
                *
                static_cast<T>(20)
            );

        const T presenceGainDb =
            static_cast<T>(0)
            +
            (
                this->presence_
                *
                static_cast<T>(12)
            );

        /**
         * Bass
         *
         * Marshall low-end is tighter
         * than Fender.
         */

        this->bassFilter_
            .setLowShelf(
                this->sampleRate_,
                static_cast<T>(120.0),
                static_cast<T>(0.707),
                bassGainDb);

        /**
         * Midrange emphasis.
         *
         * Classic Marshall growl.
         */

        this->midFilter_
            .setPeak(
                this->sampleRate_,
                static_cast<T>(700.0),
                static_cast<T>(0.8),
                midGainDb);

        /**
         * Upper-mid / treble attack.
         */

        this->trebleFilter_
            .setHighShelf(
                this->sampleRate_,
                static_cast<T>(2500.0),
                static_cast<T>(0.707),
                trebleGainDb);

        /**
         * Presence region.
         *
         * Power amp feedback style
         * brightness control.
         */

        this->presenceFilter_
            .setHighShelf(
                this->sampleRate_,
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
