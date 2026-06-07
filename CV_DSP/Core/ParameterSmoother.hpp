#ifndef CVDSP_CORE_PARAMETER_SMOOTHER_HPP
#define CVDSP_CORE_PARAMETER_SMOOTHER_HPP

/**
 * @file ParameterSmoother.hpp
 * @brief Suavizadores de parâmetros em tempo real para automação sample accurate.
 *
 * O arquivo implementa três estratégias sem alocação dinâmica:
 * - LinearSmoother: rampa linear com incremento constante.
 * - ExponentialSmoother: rampa exponencial normalizada, com chegada exata ao alvo.
 * - OnePoleSmoother: filtro passa-baixas de primeira ordem aplicado ao parâmetro.
 *
 * Compatibilidade com automação VST3 sample accurate:
 * os suavizadores são independentes de bloco. Chame setTarget() exatamente no sample
 * indicado pelo evento de automação do host e chame process() uma vez por sample.
 * Nenhuma classe aloca memória, lança exceções ou depende de estado global.
 *
 * Exemplo genérico de uso com eventos sample accurate:
 * @code{.cpp}
 * cvdsp::LinearSmoother<float> gain;
 * gain.prepare(48000.0f, 0.005f); // 5 ms
 * gain.reset(0.0f);
 *
 * for (uint32_t i = 0; i < numSamples; ++i)
 * {
 *     if (automationEventAt(i))
 *         gain.setTarget(eventValueAt(i)); // aplicado no offset exato do host
 *
 *     const float g = gain.process();
 *     left[i] *= g;
 *     right[i] *= g;
 * }
 * @endcode
 *
 * Exemplo com duração explícita em samples, útil quando o host ou plugin decide
 * o tamanho da transição por evento:
 * @code{.cpp}
 * cvdsp::ExponentialSmoother<double> cutoff;
 * cutoff.prepare(48000.0, 0.020, 6.0); // padrão: 20 ms, curva mais acentuada
 * cutoff.reset(1000.0);
 * cutoff.setTarget(8000.0, 128);        // rampa exatamente por 128 samples
 * const double hz = cutoff.process();
 * @endcode
 *
 * Exemplo de one-pole para parâmetros que podem perseguir continuamente o alvo:
 * @code{.cpp}
 * cvdsp::OnePoleSmoother<float> pan;
 * pan.prepare(48000.0f, 0.010f); // constante de tempo tau = 10 ms
 * pan.reset(0.0f);
 * pan.setTarget(1.0f);
 * const float smoothPan = pan.process();
 * @endcode
 *
 * Matemática:
 *
 * Linear Ramp:
 * dado valor inicial x0, alvo xT e N samples, o incremento é
 *     d = (xT - x0) / N.
 * A sequência é
 *     y[n] = x0 + n d, 1 <= n <= N,
 * com y[N] forçado para xT para evitar erro acumulado de ponto flutuante.
 * A velocidade da mudança é constante; a primeira derivada é descontínua nos
 * pontos de início/fim da rampa.
 *
 * Exponential Ramp:
 * esta implementação usa uma curva exponencial normalizada para funcionar com
 * valores positivos, zero, negativos e cruzamentos de sinal:
 *     p[n] = (1 - exp(-k n / N)) / (1 - exp(-k)), 1 <= n <= N,
 *     y[n] = x0 + (xT - x0) p[n].
 * k controla a curvatura; k próximo de zero se aproxima de uma rampa linear,
 * k maior gera movimento inicial mais rápido e aproximação final mais lenta.
 * Como p[N] = 1, a rampa termina exatamente em xT.
 *
 * One Pole Filter:
 * o parâmetro é filtrado por um passa-baixas de primeira ordem:
 *     y[n] = y[n-1] + a (x[n] - y[n-1]),
 *     a = 1 - exp(-1 / (tau Fs)).
 * tau é a constante de tempo em segundos e Fs é a taxa de amostragem. Após tau
 * segundos, a resposta ao degrau percorre aproximadamente 63,2% da distância
 * até o alvo. Diferente das rampas de N samples, o one-pole se aproxima
 * assintoticamente do alvo.
 */

#include "Namespace.hpp"
#include "Types.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

namespace cvdsp
{
namespace detail
{
template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr T clampToNonNegative(const T value) noexcept
{
    return value > static_cast<T>(0) ? value : static_cast<T>(0);
}

template <typename T>
CVDSP_NODISCARD CVDSP_FORCE_INLINE constexpr u32 secondsToSamples(const T sampleRate,
                                                                  const T seconds) noexcept
{
    const T safeSampleRate = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(0);
    const T safeSeconds = clampToNonNegative(seconds);
    const T samples = safeSampleRate * safeSeconds;

    if (samples <= static_cast<T>(0))
        return 0;

    constexpr T maxU32 = static_cast<T>(std::numeric_limits<u32>::max());
    if (samples >= maxU32)
        return std::numeric_limits<u32>::max();

    return static_cast<u32>(samples + static_cast<T>(0.5));
}
} // namespace detail

/**
 * @brief Rampa linear de parâmetro com duração configurável.
 *
 * @tparam T Tipo de ponto flutuante, normalmente float ou double.
 */
template <typename T = f32>
class LinearSmoother
{
    static_assert(std::is_floating_point_v<T>, "LinearSmoother requires a floating-point type");

public:
    using value_type = T;

    /**
     * @brief Configura a taxa de amostragem e a duração padrão da rampa.
     */
    CVDSP_FORCE_INLINE void prepare(const T sampleRate, const T rampTimeSeconds) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(0);
        defaultRampSamples_ = detail::secondsToSamples(sampleRate_, rampTimeSeconds);
    }

    /**
     * @brief Reinicia o estado imediatamente para value.
     */
    CVDSP_FORCE_INLINE void reset(const T value = static_cast<T>(0)) noexcept
    {
        current_ = value;
        target_ = value;
        step_ = static_cast<T>(0);
        samplesRemaining_ = 0;
    }

    /**
     * @brief Define o alvo usando a duração padrão configurada em prepare().
     */
    CVDSP_FORCE_INLINE void setTarget(const T target) noexcept
    {
        setTarget(target, defaultRampSamples_);
    }

    /**
     * @brief Define o alvo usando uma duração explícita em samples.
     */
    CVDSP_FORCE_INLINE void setTarget(const T target, const u32 rampSamples) noexcept
    {
        target_ = target;
        samplesRemaining_ = rampSamples;

        if (samplesRemaining_ == 0)
        {
            current_ = target_;
            step_ = static_cast<T>(0);
            return;
        }

        step_ = (target_ - current_) / static_cast<T>(samplesRemaining_);
    }

    /**
     * @brief Processa um sample de suavização e retorna o valor atual.
     */
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T process() noexcept
    {
        if (samplesRemaining_ == 0)
            return current_;

        --samplesRemaining_;
        if (samplesRemaining_ == 0)
        {
            current_ = target_;
            step_ = static_cast<T>(0);
            return current_;
        }

        current_ += step_;
        return current_;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentValue() const noexcept { return current_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetValue() const noexcept { return target_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isSmoothing() const noexcept { return samplesRemaining_ != 0; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE u32 getSamplesRemaining() const noexcept { return samplesRemaining_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getSampleRate() const noexcept { return sampleRate_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE u32 getDefaultRampSamples() const noexcept { return defaultRampSamples_; }

private:
    T sampleRate_ = static_cast<T>(0);
    T current_ = static_cast<T>(0);
    T target_ = static_cast<T>(0);
    T step_ = static_cast<T>(0);
    u32 defaultRampSamples_ = 0;
    u32 samplesRemaining_ = 0;
};

/**
 * @brief Rampa exponencial normalizada com chegada exata ao alvo.
 *
 * A forma normalizada evita as restrições da rampa exponencial multiplicativa
 * clássica, portanto é segura para parâmetros que podem ser zero, negativos ou
 * cruzar zero. O custo é uma chamada a std::exp por sample enquanto há rampa.
 */
template <typename T = f32>
class ExponentialSmoother
{
    static_assert(std::is_floating_point_v<T>, "ExponentialSmoother requires a floating-point type");

public:
    using value_type = T;

    /**
     * @brief Configura taxa de amostragem, duração padrão e curvatura.
     *
     * @param curve Valor positivo. Valores próximos de zero aproximam uma linha;
     *        valores maiores deixam o começo mais rápido e o fim mais suave.
     */
    CVDSP_FORCE_INLINE void prepare(const T sampleRate,
                                    const T rampTimeSeconds,
                                    const T curve = static_cast<T>(5)) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(0);
        defaultRampSamples_ = detail::secondsToSamples(sampleRate_, rampTimeSeconds);
        setCurve(curve);
    }

    CVDSP_FORCE_INLINE void reset(const T value = static_cast<T>(0)) noexcept
    {
        start_ = value;
        current_ = value;
        target_ = value;
        totalSamples_ = 0;
        samplesRemaining_ = 0;
        position_ = 0;
    }

    CVDSP_FORCE_INLINE void setTarget(const T target) noexcept
    {
        setTarget(target, defaultRampSamples_);
    }

    CVDSP_FORCE_INLINE void setTarget(const T target, const u32 rampSamples) noexcept
    {
        target_ = target;
        start_ = current_;
        totalSamples_ = rampSamples;
        samplesRemaining_ = rampSamples;
        position_ = 0;

        if (samplesRemaining_ == 0)
        {
            current_ = target_;
            start_ = target_;
        }
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T process() noexcept
    {
        if (samplesRemaining_ == 0)
            return current_;

        ++position_;
        --samplesRemaining_;

        if (samplesRemaining_ == 0 || position_ >= totalSamples_)
        {
            current_ = target_;
            start_ = target_;
            return current_;
        }

        const T phase = static_cast<T>(position_) / static_cast<T>(totalSamples_);
        const T progress = isNearlyLinear_
                               ? phase
                               : (static_cast<T>(1) - std::exp(-curve_ * phase)) * normalization_;

        current_ = start_ + (target_ - start_) * progress;
        return current_;
    }

    /**
     * @brief Atualiza apenas a curvatura das próximas rampas.
     */
    CVDSP_FORCE_INLINE void setCurve(const T curve) noexcept
    {
        curve_ = curve > static_cast<T>(0) ? curve : static_cast<T>(0);
        isNearlyLinear_ = curve_ <= static_cast<T>(1.0e-6);
        normalization_ = isNearlyLinear_
                             ? static_cast<T>(1)
                             : static_cast<T>(1) / (static_cast<T>(1) - std::exp(-curve_));
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentValue() const noexcept { return current_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetValue() const noexcept { return target_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurve() const noexcept { return curve_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE bool isSmoothing() const noexcept { return samplesRemaining_ != 0; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE u32 getSamplesRemaining() const noexcept { return samplesRemaining_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getSampleRate() const noexcept { return sampleRate_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE u32 getDefaultRampSamples() const noexcept { return defaultRampSamples_; }

private:
    T sampleRate_ = static_cast<T>(0);
    T start_ = static_cast<T>(0);
    T current_ = static_cast<T>(0);
    T target_ = static_cast<T>(0);
    T curve_ = static_cast<T>(5);
    T normalization_ = static_cast<T>(1) / (static_cast<T>(1) - std::exp(static_cast<T>(-5)));
    u32 defaultRampSamples_ = 0;
    u32 totalSamples_ = 0;
    u32 samplesRemaining_ = 0;
    u32 position_ = 0;
    bool isNearlyLinear_ = false;
};

/**
 * @brief Suavizador one-pole: filtro passa-baixas de primeira ordem.
 *
 * É ideal para parâmetros continuamente atualizados. Como a convergência é
 * assintótica, use LinearSmoother ou ExponentialSmoother quando for necessário
 * alcançar exatamente o alvo após um número fixo de samples.
 */
template <typename T = f32>
class OnePoleSmoother
{
    static_assert(std::is_floating_point_v<T>, "OnePoleSmoother requires a floating-point type");

public:
    using value_type = T;

    /**
     * @brief Configura a taxa de amostragem e a constante de tempo tau.
     */
    CVDSP_FORCE_INLINE void prepare(const T sampleRate, const T timeConstantSeconds) noexcept
    {
        sampleRate_ = sampleRate > static_cast<T>(0) ? sampleRate : static_cast<T>(0);
        timeConstantSeconds_ = detail::clampToNonNegative(timeConstantSeconds);
        updateCoefficient();
    }

    CVDSP_FORCE_INLINE void reset(const T value = static_cast<T>(0)) noexcept
    {
        current_ = value;
        target_ = value;
    }

    CVDSP_FORCE_INLINE void setTarget(const T target) noexcept
    {
        target_ = target;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T process() noexcept
    {
        current_ += coefficient_ * (target_ - current_);
        return current_;
    }

    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCurrentValue() const noexcept { return current_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTargetValue() const noexcept { return target_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getCoefficient() const noexcept { return coefficient_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getSampleRate() const noexcept { return sampleRate_; }
    CVDSP_NODISCARD CVDSP_FORCE_INLINE T getTimeConstantSeconds() const noexcept { return timeConstantSeconds_; }

private:
    CVDSP_FORCE_INLINE void updateCoefficient() noexcept
    {
        if (sampleRate_ <= static_cast<T>(0) || timeConstantSeconds_ <= static_cast<T>(0))
        {
            coefficient_ = static_cast<T>(1);
            return;
        }

        coefficient_ = static_cast<T>(1) -
                       std::exp(static_cast<T>(-1) / (timeConstantSeconds_ * sampleRate_));
    }

    T sampleRate_ = static_cast<T>(0);
    T timeConstantSeconds_ = static_cast<T>(0);
    T current_ = static_cast<T>(0);
    T target_ = static_cast<T>(0);
    T coefficient_ = static_cast<T>(1);
};
} // namespace cvdsp

#endif // CVDSP_CORE_PARAMETER_SMOOTHER_HPP
