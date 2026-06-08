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

        const T bassGainDb =
            static_cast<T>(-12)
            +
            (
                this->bass_
                *
                static_cast<T>(18)
            );

        const T midGainDb =
            static_cast<T>(-18)
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
                static_cast<T>(26)
            );

        const T presenceGainDb =
            (
                this->presence_
                *
                static_cast<T>(14)
            );

        /**
         * Bass.
         *
         * Mesa low-end remains tight
         * and controlled.
         */

        this->bassFilter_
            .setLowShelf(
                this->sampleRate_,
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

        this->midFilter_
            .setPeak(
                this->sampleRate_,
                static_cast<T>(750.0),
                static_cast<T>(0.9),
                midGainDb);

        /**
         * Treble control is extremely
         * influential in Mark amplifiers.
         *
         * It affects both brightness
         * and perceived gain structure.
         */

        this->trebleFilter_
            .setHighShelf(
                this->sampleRate_,
                static_cast<T>(2200.0),
                static_cast<T>(0.707),
                trebleGainDb);

        /**
         * Power amp presence region.
         */

        this->presenceFilter_
            .setHighShelf(
                this->sampleRate_,
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
