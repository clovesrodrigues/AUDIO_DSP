#ifndef CVDSP_GUITAR_TONESTACK_HPP
#define CVDSP_GUITAR_TONESTACK_HPP

/**
 * @file ToneStack.hpp
 * @brief Base class for guitar amplifier tone stacks.
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 *
 * - Filters/Biquad.hpp
 *
 * Optional:
 *
 * - Filters/StateVariableFilter.hpp
 *
 * Intended base for:
 *
 * - FenderToneStack
 * - MarshallToneStack
 * - MesaToneStack
 * - VoxToneStack
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "../Filters/Biquad.hpp"

namespace cvdsp
{

template<typename T>
class ToneStack
{
public:

    static_assert(
        std::is_floating_point_v<T>,
        "ToneStack requires float or double");

    virtual ~ToneStack() = default;

    virtual void prepare(
        T sampleRate)
        noexcept
    {
        sampleRate_ =
            (std::isfinite(sampleRate) && sampleRate > static_cast<T>(0))
            ? sampleRate
            : static_cast<T>(44100);

        bassFilter_.prepare(sampleRate_);
        midFilter_.prepare(sampleRate_);
        trebleFilter_.prepare(sampleRate_);
        presenceFilter_.prepare(sampleRate_);

        reset();
    }

    virtual void reset()
        noexcept
    {
        bassFilter_.reset();
        midFilter_.reset();
        trebleFilter_.reset();
        presenceFilter_.reset();
    }

    virtual T process(
        T input)
        noexcept = 0;

    virtual void setBass(
        T value)
        noexcept
    {
        bass_ = clampControl(value);
    }

    virtual void setMid(
        T value)
        noexcept
    {
        mid_ = clampControl(value);
    }

    virtual void setTreble(
        T value)
        noexcept
    {
        treble_ = clampControl(value);
    }

    virtual void setPresence(
        T value)
        noexcept
    {
        presence_ = clampControl(value);
    }

    [[nodiscard]]
    T getBass() const noexcept
    {
        return bass_;
    }

    [[nodiscard]]
    T getMid() const noexcept
    {
        return mid_;
    }

    [[nodiscard]]
    T getTreble() const noexcept
    {
        return treble_;
    }

    [[nodiscard]]
    T getPresence() const noexcept
    {
        return presence_;
    }

    [[nodiscard]]
    T getSampleRate() const noexcept
    {
        return sampleRate_;
    }

protected:

    [[nodiscard]]
    static T clampControl(
        T value) noexcept
    {
        if (!std::isfinite(value))
        {
            return static_cast<T>(0.5);
        }

        return std::clamp(
            value,
            static_cast<T>(0),
            static_cast<T>(1));
    }

    static void configurePeak(
        filters::Biquad<T>& filter,
        T frequencyHz,
        T q,
        T gainDb) noexcept
    {
        filter.setType(
            filters::BiquadType::PeakingEQ);

        filter.setFrequency(
            frequencyHz);

        filter.setQ(
            q);

        filter.setGainDB(
            gainDb);

        filter.updateCoefficients();
    }

    static void configureLowShelf(
        filters::Biquad<T>& filter,
        T frequencyHz,
        T q,
        T gainDb) noexcept
    {
        filter.setType(
            filters::BiquadType::LowShelf);

        filter.setFrequency(
            frequencyHz);

        filter.setQ(
            q);

        filter.setGainDB(
            gainDb);

        filter.updateCoefficients();
    }

    static void configureHighShelf(
        filters::Biquad<T>& filter,
        T frequencyHz,
        T q,
        T gainDb) noexcept
    {
        filter.setType(
            filters::BiquadType::HighShelf);

        filter.setFrequency(
            frequencyHz);

        filter.setQ(
            q);

        filter.setGainDB(
            gainDb);

        filter.updateCoefficients();
    }

protected:

    T sampleRate_ =
        static_cast<T>(44100);

    T bass_ =
        static_cast<T>(0.5);

    T mid_ =
        static_cast<T>(0.5);

    T treble_ =
        static_cast<T>(0.5);

    T presence_ =
        static_cast<T>(0.5);

    /**
     * Reutilizados pelas implementações derivadas.
     */

    filters::Biquad<T>
        bassFilter_;

    filters::Biquad<T>
        midFilter_;

    filters::Biquad<T>
        trebleFilter_;

    filters::Biquad<T>
        presenceFilter_;
};

using ToneStackF =
    ToneStack<float>;

using ToneStackD =
    ToneStack<double>;

} // namespace cvdsp

#endif
