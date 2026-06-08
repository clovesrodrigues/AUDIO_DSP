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
        ToneStack<T>::mid_ =
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
        ToneStack<T>::treble_ =
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
        const T bassGainDb =
            static_cast<T>(-12)
            +
            (
                this->bass_
                *
                static_cast<T>(24)
            );

        const T midGainDb =
            static_cast<T>(-15)
            +
            (
                this->mid_
                *
                static_cast<T>(15)
            );

        const T trebleGainDb =
            static_cast<T>(-12)
            +
            (
                this->treble_
                *
                static_cast<T>(24)
            );

        /**
         * Bass
         *
         * Fender low shelf.
         */

        this->bassFilter_
            .setLowShelf(
                this->sampleRate_,
                static_cast<T>(80.0),
                static_cast<T>(0.707),
                bassGainDb);

        /**
         * Mid Scoop Region.
         */

        this->midFilter_
            .setPeak(
                this->sampleRate_,
                static_cast<T>(450.0),
                static_cast<T>(0.7),
                midGainDb);

        /**
         * Treble Shelf.
         */

        this->trebleFilter_
            .setHighShelf(
                this->sampleRate_,
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
