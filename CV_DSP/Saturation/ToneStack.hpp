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
            sampleRate;

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
        bass_ = value;
    }

    virtual void setMid(
        T value)
        noexcept
    {
        mid_ = value;
    }

    virtual void setTreble(
        T value)
        noexcept
    {
        treble_ = value;
    }

    virtual void setPresence(
        T value)
        noexcept
    {
        presence_ = value;
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
