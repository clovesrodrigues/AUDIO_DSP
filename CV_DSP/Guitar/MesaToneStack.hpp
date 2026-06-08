#ifndef CVDSP_GUITAR_MESATONESTACK_HPP
#define CVDSP_GUITAR_MESATONESTACK_HPP

/**
 * @file MesaToneStack.hpp
 * @brief Mesa Boogie Mark Series Tone Stack
 *
 * Inspired by:
 *
 * - Mesa Mark I
 * - Mesa Mark II
 * - Mesa Mark III
 * - Mesa Mark IV
 * - Mesa Mark V
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
class MesaToneStack final
    : public ToneStack<T>
{
public:

    MesaToneStack() = default;

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
         * Mesa Mark topology.
         *
         * Bass
         * ->
         * Mid
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
         * Mesa Mark Series characteristics:
         *
         * Tight low end
         * Aggressive upper mids
         * Strong treble interaction
         * Wide presence range
         *
         * Tone stack positioned
         * before significant gain stages
         * in traditional Mark circuits.
         */

        const T midFocus =
            static_cast<T>(500)
            +
            (
                this->mid_
                *
                static_cast<T>(450)
            );

        const T midQ =
            static_cast<T>(0.75)
            +
            (
                this->presence_
                *
                static_cast<T>(0.35)
            );

        const T bassGainDb =
            static_cast<T>(-14)
            +
            (
                this->bass_
                *
                static_cast<T>(20)
            );

        const T midGainDb =
            static_cast<T>(-20)
            +
            (
                this->mid_
                *
                static_cast<T>(28)
            );

        const T trebleGainDb =
            static_cast<T>(-11)
            +
            (
                this->treble_
                *
                static_cast<T>(27)
            )
            +
            (
                this->presence_
                *
                static_cast<T>(2)
            );

        const T presenceGainDb =
            static_cast<T>(-1)
            +
            (
                this->presence_
                *
                static_cast<T>(15)
            );

        /**
         * Bass.
         *
         * Mesa low-end remains tight
         * and controlled.
         */

        ToneStack<T>::configureLowShelf(
            this->bassFilter_,
            static_cast<T>(90.0),
            static_cast<T>(0.707),
            bassGainDb);

        /**
         * Characteristic mid control.
         *
         * Mark amplifiers often produce
         * the famous V-shaped response
         * when mids are reduced.
         */

        ToneStack<T>::configurePeak(
            this->midFilter_,
            midFocus,
            midQ,
            midGainDb);

        /**
         * Treble control is extremely
         * influential in Mark amplifiers.
         *
         * It affects both brightness
         * and perceived gain structure.
         */

        ToneStack<T>::configureHighShelf(
            this->trebleFilter_,
            static_cast<T>(2200.0),
            static_cast<T>(0.707),
            trebleGainDb);

        /**
         * Power amp presence region.
         */

        ToneStack<T>::configureHighShelf(
            this->presenceFilter_,
            static_cast<T>(5000.0),
            static_cast<T>(0.707),
            presenceGainDb);
    }
};

using MesaToneStackF =
    MesaToneStack<float>;

using MesaToneStackD =
    MesaToneStack<double>;

} // namespace cvdsp

#endif
