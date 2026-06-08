#ifndef CVDSP_GUITAR_FENDERTONESTACK_HPP
#define CVDSP_GUITAR_FENDERTONESTACK_HPP

/**
 * @file FenderToneStack.hpp
 * @brief Fender Blackface Tone Stack
 *
 * Inspired by:
 *
 * - Twin Reverb
 * - Deluxe Reverb
 * - Super Reverb
 * - Bassman Blackface Era
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

#include <cmath>
#include <algorithm>

#include "ToneStack.hpp"

namespace cvdsp
{

template<typename T>
class FenderToneStack final
    : public ToneStack<T>
{
public:

    FenderToneStack() = default;

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
        ToneStack<T>::bass_ =
            ToneStack<T>::clampControl(
                value);

        updateFilters();
    }

    void setMid(
        T value)
        noexcept override
    {
        ToneStack<T>::mid_ =
            ToneStack<T>::clampControl(
                value);

        updateFilters();
    }

    void setTreble(
        T value)
        noexcept override
    {
        ToneStack<T>::treble_ =
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
         * Fender topology:
         *
         * Bass Shelf
         * ->
         * Mid Cut
         * ->
         * Treble Shelf
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

        return x;
    }

private:

    void updateFilters()
        noexcept
    {
        const T bassInteraction =
            this->bass_
            -
            static_cast<T>(0.5);

        const T trebleInteraction =
            this->treble_
            -
            static_cast<T>(0.5);

        const T bassGainDb =
            static_cast<T>(-14)
            +
            (
                this->bass_
                *
                static_cast<T>(22)
            )
            -
            (
                trebleInteraction
                *
                static_cast<T>(2)
            );

        const T midGainDb =
            static_cast<T>(-18)
            +
            (
                this->mid_
                *
                static_cast<T>(20)
            )
            +
            (
                bassInteraction
                *
                static_cast<T>(2)
            )
            -
            (
                trebleInteraction
                *
                static_cast<T>(3)
            );

        const T trebleGainDb =
            static_cast<T>(-13)
            +
            (
                this->treble_
                *
                static_cast<T>(24)
            )
            -
            (
                bassInteraction
                *
                static_cast<T>(2)
            );

        /**
         * Bass
         *
         * Fender low shelf.
         */

        ToneStack<T>::configureLowShelf(
            this->bassFilter_,
            static_cast<T>(80.0),
            static_cast<T>(0.707),
            bassGainDb);

        /**
         * Mid Scoop Region.
         */

        ToneStack<T>::configurePeak(
            this->midFilter_,
            static_cast<T>(450.0),
            static_cast<T>(0.7),
            midGainDb);

        /**
         * Treble Shelf.
         */

        ToneStack<T>::configureHighShelf(
            this->trebleFilter_,
            static_cast<T>(4000.0),
            static_cast<T>(0.707),
            trebleGainDb);
    }
};

using FenderToneStackF =
    FenderToneStack<float>;

using FenderToneStackD =
    FenderToneStack<double>;

} // namespace cvdsp

#endif
