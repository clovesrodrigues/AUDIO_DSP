#ifndef CVDSP_FILTERS_ONEPOLEFILTER_HPP
#define CVDSP_FILTERS_ONEPOLEFILTER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <limits>
#include <array>
#include <cassert>

/**
 * @file OnePoleFilter.hpp
 * @brief One-pole lowpass and highpass filters for CV_DSP
 * @namespace cvdsp::filters
 *
 * Implementação simples e numericamente estável de filtros de um pólo
 * (first-order) adequados para uso em tempo real. Projetados para serem
 * header-only, C++20, sem dependências externas, e real-time safe.
 *
 * Design decisions:
 * - Usamos o modelo discreto de "exponential smoothing" para o filtro
 *   passa-baixas: y[n] = (1 - alpha) * x[n] + alpha * y[n-1],
 *   com alpha = exp(-2*pi*fc / fs).
 * - O passa-altas é implementado como complemento: hp[n] = x[n] - lp[n].
 *
 * Razões técnicas:
 * - alpha obtido por discretização exponencial (equivalente à solução
 *   do diferencial y' + w0*y = w0*x, com w0 = 2*pi*fc) produz um filtro
 *   uniparamétrico estável e eficiente.
 * - A implementação evita divisions caras no loop crítico: apenas usa
 *   multiplicações e adições por amostra.
 *
 * Nota: Para aplicações que requerem resposta de fase exata ou Q controlado,
 *       usar desenho por bilinear transform com pré-warping. Aqui priorizamos
 *       estabilidade, simplicidade e baixo custo computacional.
 */
namespace cvdsp::filters
{

/**
 * @brief Base one-pole filter storage and simple processing primitives.
 *
 * Este tipo não é exposto publicamente como filtro por sí só — ele provê
 * armazenamento de estado e operações atômicas seguras em tempo real.
 *
 * Template:
 * - T: tipo de ponto flutuante (float ou double)
 */
template<typename T>
struct OnePole
{
    static_assert(std::is_floating_point_v<T>, "OnePole requires floating point type");

    using value_type = T;
    using size_type = std::size_t;

    // Coeficientes:
    // lp: y[n] = b0 * x[n] + a1 * y[n-1]
    value_type b0{}; ///< feedforward coefficient (1 - alpha)
    value_type a1{}; ///< feedback coefficient (alpha)

    // Estado:
    value_type z1{}; ///< estado interno y[n-1]

    constexpr OnePole() noexcept : b0(static_cast<value_type>(1)), a1(static_cast<value_type>(0)), z1(static_cast<value_type>(0)) {}

    constexpr void reset() noexcept { z1 = static_cast<value_type>(0); }

    // Processa uma amostra com os coeficientes atuais (lowpass primitive)
    inline value_type processLP(const value_type x) noexcept
    {
        // y = b0 * x + a1 * z1
        const value_type y = b0 * x + a1 * z1;
        z1 = y; // atualizar estado
        return y;
    }
};

/**
 * @class LowPassOnePole
 * @brief Filtro passa-baixas de um pólo (first-order low-pass)
 *
 * Características:
 * - y[n] = (1 - alpha) * x[n] + alpha * y[n-1]
 * - alpha = exp(-2*pi*fc / fs)
 *
 * Métodos principais:
 * - prepare(sampleRate)
 * - reset()
 * - setCutoff(freqHz)
 * - process(sample)
 */
template<typename T>
class LowPassOnePole
{
public:
    static_assert(std::is_floating_point_v<T>, "LowPassOnePole requires floating point type");
    using value_type = T;
    using size_type = std::size_t;

    constexpr LowPassOnePole() noexcept = default;
    ~LowPassOnePole() noexcept = default;

    LowPassOnePole(const LowPassOnePole&) = delete;
    LowPassOnePole& operator=(const LowPassOnePole&) = delete;

    /**
     * @brief Prepara o filtro definindo a taxa de amostragem.
     *
     * Deve ser chamado fora do audio thread. Zera estado interno.
     */
    inline void prepare(size_type sampleRate) noexcept
    {
        assert(sampleRate > 0);
        m_sampleRate = sampleRate;
        m_initialized = true;
        m_onePole.reset();
        // define default cutoff (nyquist/2) conservador
        setCutoffHz(static_cast<value_type>(m_sampleRate) / static_cast<value_type>(2));
    }

    /**
     * @brief Reseta o estado interno do filtro (zera histórico).
     */
    inline void reset() noexcept
    {
        m_onePole.reset();
    }

    /**
     * @brief Define a frequência de corte em Hz.
     *
     * Real-time safe; O(1). Não faz alocações.
     */
    inline void setCutoffHz(value_type freqHz) noexcept
    {
        // segura limites: [minFreq, nyquist - tiny]
        const value_type minFreq = static_cast<value_type>(1e-6);
        const value_type nyquist = static_cast<value_type>(m_sampleRate) * static_cast<value_type>(0.5);
        const value_type f = std::clamp(freqHz, minFreq, nyquist - static_cast<value_type>(1e-12));

        // alpha = exp(-2*pi*fc / fs)
        // para estabilidade numérica, quando fc >> fs/2, f clamped
        const value_type omega = static_cast<value_type>(2.0) * static_cast<value_type>(M_PI) * f / static_cast<value_type>(m_sampleRate);
        const value_type alpha = std::exp(-omega);

        m_cutoffHz = f;
        m_onePole.a1 = alpha;
        m_onePole.b0 = static_cast<value_type>(1) - alpha;
    }

    /**
     * @brief Retorna a frequência de corte atual em Hz.
     */
    inline value_type getCutoffHz() const noexcept { return m_cutoffHz; }

    /**
     * @brief Processa uma amostra e retorna a saída do filtro passa-baixas.
     *
     * Real-time safe: O(1), sem alocações, sem exceções.
     */
    inline value_type process(value_type x) noexcept
    {
        return m_onePole.processLP(x);
    }

private:
    OnePole<value_type> m_onePole{};
    size_type m_sampleRate{48000};
    value_type m_cutoffHz{static_cast<value_type>(24000)}; // default
    bool m_initialized{false};
};

/**
 * @class HighPassOnePole
 * @brief Filtro passa-altas de um pólo implementado como x - lowpass(x)
 *
 * Implementação:
 * - y_hp[n] = x[n] - y_lp[n]
 * - onde y_lp[n] é saída do LowPassOnePole com mesma alpha
 *
 * Esta formulação garante estabilidade e é eficiente: usa os mesmos coeficientes
 * alpha do lowpass e um estado adicional para x[n-1] não é necessário aqui.
 */
template<typename T>
class HighPassOnePole
{
public:
    static_assert(std::is_floating_point_v<T>, "HighPassOnePole requires floating point type");
    using value_type = T;
    using size_type = std::size_t;

    constexpr HighPassOnePole() noexcept = default;
    ~HighPassOnePole() noexcept = default;

    HighPassOnePole(const HighPassOnePole&) = delete;
    HighPassOnePole& operator=(const HighPassOnePole&) = delete;

    /**
     * @brief Prepara o filtro com a taxa de amostragem.
     */
    inline void prepare(size_type sampleRate) noexcept
    {
        assert(sampleRate > 0);
        m_sampleRate = sampleRate;
        m_initialized = true;
        reset();
    }

    /**
     * @brief Reseta o estado interno do filtro.
     */
    inline void reset() noexcept
    {
        m_lpState = static_cast<value_type>(0);
    }

    /**
     * @brief Define a frequência de corte em Hz para o passa-altas.
     *
     * Internamente configura o lowpass complementar com o mesmo alpha.
     */
    inline void setCutoffHz(value_type freqHz) noexcept
    {
        m_cutoffHz = freqHz;
        // Calcula coeficiente do lowpass interno
        // Reusa a fórmula do LowPassOnePole
        const value_type minFreq = static_cast<value_type>(1e-6);
        const value_type nyquist = static_cast<value_type>(m_sampleRate) * static_cast<value_type>(0.5);
        const value_type f = std::clamp(freqHz, minFreq, nyquist - static_cast<value_type>(1e-12));
        const value_type omega = static_cast<value_type>(2.0) * static_cast<value_type>(M_PI) * f / static_cast<value_type>(m_sampleRate);
        const value_type alpha = std::exp(-omega);

        // lowpass coefficients: b0 = 1 - alpha, a1 = alpha
        m_lpCoeffa1 = alpha;
        m_lpCoeffb0 = static_cast<value_type>(1) - alpha;
    }

    inline value_type getCutoffHz() const noexcept { return m_cutoffHz; }

    /**
     * @brief Processa uma amostra e retorna a saída do passa-altas.
     *
     * Implementação:
     * - primeiro calcula lowpass usando os coeficientes armazenados
     * - hp = x - lp
     * - atualiza estado do lowpass
     */
    inline value_type process(value_type x) noexcept
    {
        // lowpass: lp = b0 * x + a1 * z1
        const value_type lp = m_lpCoeffb0 * x + m_lpCoeffa1 * m_lpState;
        // hp = x - lp
        const value_type hp = x - lp;
        // atualiza estado do lowpass
        m_lpState = lp;
        return hp;
    }

private:
    // estado do lowpass interno
    value_type m_lpState{};
    value_type m_lpCoeffb0{static_cast<value_type>(1)};
    value_type m_lpCoeffa1{static_cast<value_type>(0)};

    size_type m_sampleRate{48000};
    value_type m_cutoffHz{static_cast<value_type>(20)};
    bool m_initialized{false};
};

} // namespace cvdsp::filters

#endif // CVDSP_FILTERS_ONEPOLEFILTER_HPP
