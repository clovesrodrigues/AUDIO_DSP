#ifndef CVDSP_DYNAMICS_EXPANDER_HPP
#define CVDSP_DYNAMICS_EXPANDER_HPP

/**
 * @file Expander.hpp
 * @brief Downward Expander
 *
 * Header-only
 * Real-Time Safe
 * C++20
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace cvdsp
{

/**
 * @brief Downward Expander
 *
 * Threshold em dBFS
 * Ratio >= 1
 *
 * Exemplo:
 *
 * threshold = -40 dB
 * ratio = 4
 *
 * Sinais abaixo do threshold
 * serão progressivamente atenuados.
 */
template<typename T>
class Expander
{
    static_assert(
        std::is_floating_point_v<T>,
        "Expander requires floating point type");

public:

    constexpr Expander() noexcept = default;

    /**
     * @brief Inicializa coeficientes.
     *
     * @param sampleRate Taxa de amostragem
     * @param attackMs Ataque em ms
     * @param releaseMs Release em ms
     * @param thresholdDb Threshold em dBFS
     * @param ratio Ratio de expansão
     */
    void prepare(
        T sampleRate,
        T attackMs,
        T releaseMs,
        T thresholdDb,
        T ratio) noexcept
    {
        sampleRate_ = sampleRate;

        attackMs_ = std::max(
            attackMs,
            static_cast<T>(0.001));

        releaseMs_ = std::max(
            releaseMs,
            static_cast<T>(0.001));

        thresholdDb_ = thresholdDb;

        ratio_ = std::max(
            ratio,
            static_cast<T>(1));

        attackCoeff_ =
            computeTimeCoefficient(
                attackMs_);

        releaseCoeff_ =
            computeTimeCoefficient(
                releaseMs_);

        reset();
    }

    /**
     * @brief Reinicia estado interno.
     */
    void reset() noexcept
    {
        envelopeDb_ = static_cast<T>(-120);
        gainDb_ = static_cast<T>(0);
    }

    /**
     * @brief Processa uma amostra.
     *
     * @param input Entrada
     * @return Saída expandida
     */
    inline T process(T input) noexcept
    {
        constexpr T kMinDb =
            static_cast<T>(-120);

        constexpr T kEpsilon =
            static_cast<T>(1e-30);

        const T absInput =
            std::abs(input);

        const T levelDb =
            static_cast<T>(20) *
            std::log10(
                std::max(
                    absInput,
                    kEpsilon));

        const T detectorCoeff =
            (levelDb > envelopeDb_)
                ? attackCoeff_
                : releaseCoeff_;

        envelopeDb_ =
            detectorCoeff * envelopeDb_
            +
            (static_cast<T>(1) - detectorCoeff)
            * levelDb;

        T targetGainDb =
            static_cast<T>(0);

        if (envelopeDb_ < thresholdDb_)
        {
            const T delta =
                thresholdDb_ - envelopeDb_;

            targetGainDb =
                -delta *
                (ratio_ - static_cast<T>(1));
        }

        const T gainCoeff =
            (targetGainDb < gainDb_)
                ? attackCoeff_
                : releaseCoeff_;

        gainDb_ =
            gainCoeff * gainDb_
            +
            (static_cast<T>(1) - gainCoeff)
            * targetGainDb;

        gainDb_ =
            std::clamp(
                gainDb_,
                kMinDb,
                static_cast<T>(0));

        const T linearGain =
            dbToLinear(
                gainDb_);

        return input * linearGain;
    }

private:

    /**
     * @brief Converte dB para ganho linear.
     */
    static constexpr T dbToLinear(
        T db) noexcept
    {
        return std::pow(
            static_cast<T>(10),
            db / static_cast<T>(20));
    }

    /**
     * @brief Coeficiente exponencial.
     */
    T computeTimeCoefficient(
        T timeMs) const noexcept
    {
        const T timeSeconds =
            timeMs *
            static_cast<T>(0.001);

        return std::exp(
            static_cast<T>(-1)
            /
            (
                sampleRate_
                *
                timeSeconds
            ));
    }

private:

    T sampleRate_  = static_cast<T>(44100);

    T thresholdDb_ = static_cast<T>(-40);

    T ratio_       = static_cast<T>(4);

    T attackMs_    = static_cast<T>(5);

    T releaseMs_   = static_cast<T>(100);

    T attackCoeff_ = static_cast<T>(0);

    T releaseCoeff_ = static_cast<T>(0);

    T envelopeDb_  = static_cast<T>(-120);

    T gainDb_      = static_cast<T>(0);
};

} // namespace cvdsp

#endif
