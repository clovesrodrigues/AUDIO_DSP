#ifndef CVDSP_SPECTRAL_WINDOWFUNCTIONS_HPP
#define CVDSP_SPECTRAL_WINDOWFUNCTIONS_HPP

/**
 * @file WindowFunctions.hpp
 * @brief Spectral Window Functions
 *
 * Header-Only
 * C++20
 * Real-Time Safe
 *
 * Dependencies:
 * - FFT.hpp
 *
 * Supported Windows:
 * - Rectangular
 * - Hann
 * - Hamming
 * - Blackman
 * - Blackman-Harris
 * - Kaiser
 */

#include <array>
#include <cstddef>
#include <cmath>
#include <type_traits>


namespace cvdsp::spectral
{

/**
 * @brief Supported window types.
 */
enum class WindowType
{
    Rectangular,
    Hann,
    Hamming,
    Blackman,
    BlackmanHarris,
    Kaiser
};

/**
 * @brief Window generator.
 *
 * Template parameter N defines the
 * window size at compile time.
 */
template<typename T, std::size_t N>
class WindowFunctions
{
    static_assert(
        std::is_floating_point_v<T>,
        "WindowFunctions requires floating point type");

public:

    using WindowBuffer =
        std::array<T, N>;

public:

    /**
     * @brief Generate window coefficients.
     *
     * Kaiser beta is ignored by all
     * other window types.
     */
    [[nodiscard]]
    static WindowBuffer generate(
        WindowType type,
        T beta =
            static_cast<T>(8.6)) noexcept
    {
        WindowBuffer window{};

        switch(type)
        {
            case WindowType::Rectangular:
                generateRectangular(window);
                break;

            case WindowType::Hann:
                generateHann(window);
                break;

            case WindowType::Hamming:
                generateHamming(window);
                break;

            case WindowType::Blackman:
                generateBlackman(window);
                break;

            case WindowType::BlackmanHarris:
                generateBlackmanHarris(window);
                break;

            case WindowType::Kaiser:
                generateKaiser(
                    window,
                    beta);
                break;
        }

        return window;
    }

    /**
     * @brief Apply window in-place.
     */
    static void apply(
        T* samples,
        const WindowBuffer& window) noexcept
    {
        for (std::size_t i = 0;
             i < N;
             ++i)
        {
            samples[i] *=
                window[i];
        }
    }

    /**
     * @brief Apply window from source
     * to destination.
     */
    static void apply(
        const T* input,
        T* output,
        const WindowBuffer& window) noexcept
    {
        for (std::size_t i = 0;
             i < N;
             ++i)
        {
            output[i] =
                input[i]
                *
                window[i];
        }
    }

private:

    static constexpr T pi() noexcept
    {
        return static_cast<T>(
            3.1415926535897932384626433832795);
    }

    static void generateRectangular(
        WindowBuffer& w) noexcept
    {
        for (auto& v : w)
        {
            v =
                static_cast<T>(1);
        }
    }

    static void generateHann(
        WindowBuffer& w) noexcept
    {
        constexpr T twoPi =
            static_cast<T>(2)
            *
            pi();

        for (std::size_t n = 0;
             n < N;
             ++n)
        {
            w[n] =
                static_cast<T>(0.5)
                -
                static_cast<T>(0.5)
                *
                std::cos(
                    twoPi
                    *
                    static_cast<T>(n)
                    /
                    static_cast<T>(N - 1));
        }
    }

    static void generateHamming(
        WindowBuffer& w) noexcept
    {
        constexpr T twoPi =
            static_cast<T>(2)
            *
            pi();

        for (std::size_t n = 0;
             n < N;
             ++n)
        {
            w[n] =
                static_cast<T>(0.54)
                -
                static_cast<T>(0.46)
                *
                std::cos(
                    twoPi
                    *
                    static_cast<T>(n)
                    /
                    static_cast<T>(N - 1));
        }
    }

    static void generateBlackman(
        WindowBuffer& w) noexcept
    {
        constexpr T a0 =
            static_cast<T>(0.42);

        constexpr T a1 =
            static_cast<T>(0.50);

        constexpr T a2 =
            static_cast<T>(0.08);

        constexpr T twoPi =
            static_cast<T>(2)
            *
            pi();

        constexpr T fourPi =
            static_cast<T>(4)
            *
            pi();

        for (std::size_t n = 0;
             n < N;
             ++n)
        {
            const T phase =
                static_cast<T>(n)
                /
                static_cast<T>(N - 1);

            w[n] =
                a0
                -
                a1
                *
                std::cos(
                    twoPi * phase)
                +
                a2
                *
                std::cos(
                    fourPi * phase);
        }
    }

    static void generateBlackmanHarris(
        WindowBuffer& w) noexcept
    {
        constexpr T a0 =
            static_cast<T>(0.35875);

        constexpr T a1 =
            static_cast<T>(0.48829);

        constexpr T a2 =
            static_cast<T>(0.14128);

        constexpr T a3 =
            static_cast<T>(0.01168);

        constexpr T twoPi =
            static_cast<T>(2)
            *
            pi();

        constexpr T fourPi =
            static_cast<T>(4)
            *
            pi();

        constexpr T sixPi =
            static_cast<T>(6)
            *
            pi();

        for (std::size_t n = 0;
             n < N;
             ++n)
        {
            const T phase =
                static_cast<T>(n)
                /
                static_cast<T>(N - 1);

            w[n] =
                a0
                -
                a1
                *
                std::cos(
                    twoPi * phase)
                +
                a2
                *
                std::cos(
                    fourPi * phase)
                -
                a3
                *
                std::cos(
                    sixPi * phase);
        }
    }

    static void generateKaiser(
        WindowBuffer& w,
        T beta) noexcept
    {
        const T denom =
            besselI0(beta);

        for (std::size_t n = 0;
             n < N;
             ++n)
        {
            const T ratio =
                (
                    static_cast<T>(2)
                    *
                    static_cast<T>(n)
                )
                /
                static_cast<T>(N - 1)
                -
                static_cast<T>(1);

            const T value =
                beta
                *
                std::sqrt(
                    static_cast<T>(1)
                    -
                    ratio * ratio);

            w[n] =
                besselI0(value)
                /
                denom;
        }
    }

    /**
     * @brief Modified Bessel I0.
     *
     * Polynomial approximation.
     */
    [[nodiscard]]
    static T besselI0(
        T x) noexcept
    {
        T sum =
            static_cast<T>(1);

        T term =
            static_cast<T>(1);

        const T xx =
            x * x
            *
            static_cast<T>(0.25);

        for (std::size_t k = 1;
             k < 30;
             ++k)
        {
            term *=
                xx
                /
                (
                    static_cast<T>(k)
                    *
                    static_cast<T>(k));

            sum += term;
        }

        return sum;
    }
};

} // namespace cvdsp::spectral

namespace cvdsp
{
using WindowType = spectral::WindowType;

template<typename T, std::size_t N>
using WindowFunctions = spectral::WindowFunctions<T, N>;
} // namespace cvdsp

#endif
