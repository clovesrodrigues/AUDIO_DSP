#ifndef CVDSP_DYNAMICS_ENVELOPEFOLLOWER_HPP
#define CVDSP_DYNAMICS_ENVELOPEFOLLOWER_HPP

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <algorithm>

#include "../Core/Types.hpp"

/**
 * @file EnvelopeFollower.hpp
 * @brief Peak / RMS envelope follower (detector) for CV_DSP.
 * @namespace cvdsp::dynamics
 *
 * @section overview Visão geral
 *
 * Um *envelope follower* (detector de envelope) estima a amplitude
 * instantânea — a "energia" — de um sinal de áudio ao longo do tempo. É o
 * bloco de medição que alimenta processadores de dinâmica (compressores,
 * limiters, gates, expanders), moduladores controlados por amplitude (auto-wah,
 * envelope filters) e medidores de nível.
 *
 * Esta implementação oferece dois modos de detecção — @b Peak e @b RMS — com
 * tempos de @b ataque e @b release independentes, todos parametrizados em
 * milissegundos e convertidos em coeficientes de suavização de um pólo a partir
 * da taxa de amostragem.
 *
 * Segue as regras do CV_DSP: header-only, C++20, sem dependências externas,
 * real-time safe (sem alocação, exceções ou RTTI em `process()`),
 * `template<typename T>` para `float` e `double`, com Doxygen completo.
 *
 * @section peak Peak detection (detecção de pico)
 *
 * A detecção de pico retifica o sinal (valor absoluto) e suaviza o resultado
 * com um filtro de um pólo assimétrico (ataque rápido, release lento):
 *
 * @f[
 *     d[n] = |x[n]|
 * @f]
 * @f[
 *     e[n] =
 *     \begin{cases}
 *        \alpha_a\, e[n-1] + (1-\alpha_a)\, d[n], & d[n] > e[n-1] \;\text{(ataque)}\\[4pt]
 *        \alpha_r\, e[n-1] + (1-\alpha_r)\, d[n], & d[n] \le e[n-1] \;\text{(release)}
 *     \end{cases}
 * @f]
 *
 * O envelope @b sobe rápido quando o sinal cresce (coeficiente de ataque) e
 * @b desce devagar quando o sinal cai (coeficiente de release). Isso captura
 * transientes sem "soltar" o nível bruscamente.
 *
 * @section rms RMS detection (detecção RMS)
 *
 * A detecção RMS estima a raiz do valor quadrático médio, que corresponde à
 * energia percebida (mais próxima da audição que o pico). Suaviza-se o
 * @b quadrado do sinal e extrai-se a raiz:
 *
 * @f[
 *     d[n] = x^2[n]
 * @f]
 * @f[
 *     m[n] =
 *     \begin{cases}
 *        \alpha_a\, m[n-1] + (1-\alpha_a)\, d[n], & d[n] > m[n-1]\\
 *        \alpha_r\, m[n-1] + (1-\alpha_r)\, d[n], & d[n] \le m[n-1]
 *     \end{cases}
 * @f]
 * @f[
 *     e[n] = \sqrt{\,m[n]\,}
 * @f]
 *
 * O estado interno @f$ m[n] @f$ é a média móvel exponencial do sinal ao
 * quadrado (o "mean square"); a saída é a sua raiz (o "root mean square").
 *
 * @section coeffs Conversão tempo → coeficiente
 *
 * Cada coeficiente vem da resposta de um filtro de um pólo de primeira ordem.
 * Para uma constante de tempo @f$ \tau @f$ (em segundos), o coeficiente é:
 *
 * @f[
 *     \alpha = e^{-1/(\tau f_s)}, \qquad \tau = \text{timeMs}\times 10^{-3}
 * @f]
 *
 * Interpretação: após @f$ \tau @f$ segundos, o envelope percorre
 * @f$ 1 - 1/e \approx 63.2\% @f$ da distância até o novo valor — esta é a
 * definição clássica de "tempo de ataque/release" por constante de tempo.
 * Tempos menores ⇒ @f$ \alpha @f$ menor ⇒ resposta mais rápida;
 * @f$ \text{timeMs}=0 @f$ ⇒ @f$ \alpha=0 @f$ ⇒ resposta instantânea.
 *
 * @section tracking Envelope tracking (gráficos conceituais)
 *
 * Comportamento ataque rápido / release lento sobre uma rajada de sinal:
 *
 * @verbatim
 *   |x[n]|  (retificado)            envelope e[n]
 *
 *   amp                              amp
 *    ^                                ^
 *    |   ____                         |      .-''''''-.._
 *    |  |    |                        |   .-'           ''-.._
 *    |  |    |                        | .'                    ''----____
 *    |__|    |________   t            |/__________________________________ t
 *      attack  release                 ataque rápido     release lento
 *
 *   Peak  : segue a crista (|x|), reage a transientes individuais.
 *   RMS   : segue a energia (sqrt(mean(x^2))), mais suave e "musical".
 * @endverbatim
 *
 * @section rt Garantias de tempo real
 *
 * - `process()` é O(1), `noexcept`, sem alocação/exceções/RTTI.
 * - Apenas uma comparação, duas multiplicações e uma soma por amostra
 *   (mais um `sqrt` no modo RMS).
 * - Proteção contra denormais no estado do envelope.
 */
namespace cvdsp::dynamics
{

/**
 * @brief Modo de detecção do @ref EnvelopeFollower.
 */
enum class EnvelopeMode
{
    Peak, ///< Detecção de pico: suaviza |x[n]|.
    RMS   ///< Detecção RMS: suaviza x^2[n] e retorna a raiz.
};

/**
 * @class EnvelopeFollower
 * @brief Detector de envelope Peak/RMS com ataque e release independentes.
 *
 * @tparam T Tipo de ponto flutuante do processamento (`float` ou `double`).
 *
 * @par Exemplo — detector de pico para um VU/medidor:
 * @code
 *   cvdsp::dynamics::EnvelopeFollower<float> env;
 *   env.prepare(48000.0f);
 *   env.setMode(cvdsp::dynamics::EnvelopeMode::Peak);
 *   env.setAttackMs(1.0f);    // ataque rápido (1 ms)
 *   env.setReleaseMs(100.0f); // release suave (100 ms)
 *   for (std::size_t n = 0; n < numSamples; ++n)
 *   {
 *       float level = env.process(buffer[n]); // 0..~1 (linear)
 *       // usar 'level' para iluminar um medidor, etc.
 *   }
 * @endcode
 *
 * @par Exemplo — detector RMS alimentando um ganho de compressor:
 * @code
 *   cvdsp::dynamics::EnvelopeFollower<double> det;
 *   det.prepare(96000.0);
 *   det.setMode(cvdsp::dynamics::EnvelopeMode::RMS);
 *   det.setAttackMs(10.0);
 *   det.setReleaseMs(200.0);
 *   double rms = det.process(x);
 *   double rmsDB = 20.0 * std::log10(std::max(rms, 1e-9));
 *   // computar redução de ganho a partir de rmsDB...
 * @endcode
 *
 * @par Exemplo — auto-wah (envelope controla a cutoff):
 * @code
 *   float e = env.process(guitarSample);   // envelope da guitarra
 *   float cutoff = 300.0f + 4000.0f * e;   // mapeia envelope -> Hz
 *   // alimentar 'cutoff' em um Biquad/SVF...
 * @endcode
 */
template <typename T>
class EnvelopeFollower
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "EnvelopeFollower requires a floating point type (float or double)");

    /// @brief Tipo escalar de ponto flutuante usado pelo detector.
    using value_type = T;
    /// @brief Tipo inteiro sem sinal para contagens e índices.
    using size_type = std::size_t;

    /**
     * @brief Constrói um detector com tempos padrão e estado zerado.
     *
     * Padrões: modo Peak, ataque 10 ms, release 100 ms. Os coeficientes só são
     * válidos após @ref prepare definir a taxa de amostragem. Não aloca.
     */
    constexpr EnvelopeFollower() noexcept = default;

    /// @brief Destrutor trivial; nada é alocado.
    ~EnvelopeFollower() noexcept = default;

    EnvelopeFollower(const EnvelopeFollower&) noexcept = default;
    EnvelopeFollower& operator=(const EnvelopeFollower&) noexcept = default;
    EnvelopeFollower(EnvelopeFollower&&) noexcept = default;
    EnvelopeFollower& operator=(EnvelopeFollower&&) noexcept = default;

    /**
     * @brief Prepara o detector para uma taxa de amostragem.
     *
     * Deve ser chamado fora do audio thread. Armazena @f$ f_s @f$, zera o
     * estado e recalcula os coeficientes de ataque e release a partir dos
     * tempos (ms) atuais.
     *
     * @param sampleRate Taxa de amostragem em Hz (> 0; inválidos viram 48 kHz).
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);
        reset();
        m_attackCoeff = coeffFromMs(m_attackMs);
        m_releaseCoeff = coeffFromMs(m_releaseMs);
    }

    /**
     * @brief Zera o estado interno do envelope.
     *
     * Real-time safe: O(1), sem alocação/exceções.
     */
    inline void reset() noexcept
    {
        m_state = static_cast<value_type>(0);
    }

    /**
     * @brief Define o tempo de ataque em milissegundos.
     *
     * Recalcula apenas o coeficiente de ataque. @f$ 0 @f$ = instantâneo.
     * @param attackMs Tempo de ataque (ms, >= 0).
     */
    inline void setAttackMs(value_type attackMs) noexcept
    {
        m_attackMs = std::max(attackMs, static_cast<value_type>(0));
        m_attackCoeff = coeffFromMs(m_attackMs);
    }

    /**
     * @brief Define o tempo de release em milissegundos.
     *
     * Recalcula apenas o coeficiente de release. @f$ 0 @f$ = instantâneo.
     * @param releaseMs Tempo de release (ms, >= 0).
     */
    inline void setReleaseMs(value_type releaseMs) noexcept
    {
        m_releaseMs = std::max(releaseMs, static_cast<value_type>(0));
        m_releaseCoeff = coeffFromMs(m_releaseMs);
    }

    /**
     * @brief Define o modo de detecção (Peak ou RMS).
     *
     * @param mode Novo modo (ver @ref EnvelopeMode).
     *
     * @note Os estados internos de Peak e RMS têm escalas distintas (|x| vs
     *       x^2). Trocar de modo em tempo de execução pode produzir um degrau;
     *       chame @ref reset se desejar reiniciar o seguimento.
     */
    inline void setMode(EnvelopeMode mode) noexcept { m_mode = mode; }

    /**
     * @brief Processa uma amostra e retorna o valor atual do envelope (linear).
     *
     * No modo Peak retorna o envelope de @f$ |x| @f$; no modo RMS retorna
     * @f$ \sqrt{m[n]} @f$.
     *
     * @param x Amostra de entrada @f$ x[n] @f$.
     * @return Valor do envelope @f$ e[n] \ge 0 @f$ (escala linear).
     *
     * Real-time safe: O(1), `noexcept`, sem alocação/exceções/RTTI.
     */
    [[nodiscard]] inline value_type process(value_type x) noexcept
    {
        // Sinal do detector: |x| (Peak) ou x^2 (RMS).
        const value_type d = (m_mode == EnvelopeMode::Peak)
                                 ? std::abs(x)
                                 : x * x;

        // Coeficiente assimétrico: ataque ao subir, release ao descer.
        const value_type coeff = (d > m_state) ? m_attackCoeff
                                               : m_releaseCoeff;

        // Suavização de um pólo: e = coeff*e + (1-coeff)*d.
        m_state = coeff * m_state +
                  (static_cast<value_type>(1) - coeff) * d;

        // Proteção contra denormais no estado.
        m_state += kDenormalGuard;
        m_state -= kDenormalGuard;

        return (m_mode == EnvelopeMode::Peak) ? m_state
                                              : std::sqrt(m_state);
    }

    /**
     * @brief Retorna o valor atual do envelope sem processar nova amostra.
     * @return Envelope corrente (linear), já com a raiz aplicada no modo RMS.
     */
    [[nodiscard]] inline value_type getEnvelope() const noexcept
    {
        return (m_mode == EnvelopeMode::Peak) ? m_state
                                              : std::sqrt(m_state);
    }

    /// @brief Modo de detecção atual.
    [[nodiscard]] constexpr EnvelopeMode getMode() const noexcept { return m_mode; }
    /// @brief Tempo de ataque configurado (ms).
    [[nodiscard]] constexpr value_type getAttackMs() const noexcept { return m_attackMs; }
    /// @brief Tempo de release configurado (ms).
    [[nodiscard]] constexpr value_type getReleaseMs() const noexcept { return m_releaseMs; }
    /// @brief Coeficiente de ataque em uso.
    [[nodiscard]] constexpr value_type getAttackCoeff() const noexcept { return m_attackCoeff; }
    /// @brief Coeficiente de release em uso.
    [[nodiscard]] constexpr value_type getReleaseCoeff() const noexcept { return m_releaseCoeff; }
    /// @brief Taxa de amostragem configurada (Hz).
    [[nodiscard]] constexpr value_type getSampleRate() const noexcept { return m_sampleRate; }

private:
    /**
     * @brief Converte um tempo (ms) em coeficiente de um pólo.
     *
     * @f$ \alpha = e^{-1/(\tau f_s)} @f$, com @f$ \tau = \text{timeMs}/1000 @f$.
     * Para @f$ \text{timeMs} \le 0 @f$ retorna 0 (resposta instantânea),
     * evitando divisão por zero.
     *
     * @param timeMs Tempo em milissegundos (>= 0).
     * @return Coeficiente @f$ \alpha \in [0, 1) @f$.
     */
    [[nodiscard]] inline value_type coeffFromMs(value_type timeMs) const noexcept
    {
        if (timeMs <= static_cast<value_type>(0))
            return static_cast<value_type>(0);

        const value_type tauSamples =
            (timeMs * static_cast<value_type>(0.001)) * m_sampleRate;
        // alpha = exp(-1 / (tau_in_samples)).
        return std::exp(static_cast<value_type>(-1) / tauSamples);
    }

    /// @brief Constante anti-denormal para o estado do envelope.
    static constexpr value_type kDenormalGuard =
        static_cast<value_type>(1e-20);

    EnvelopeMode m_mode{EnvelopeMode::Peak};                 ///< Modo atual.
    value_type m_attackMs{static_cast<value_type>(10)};      ///< Ataque (ms).
    value_type m_releaseMs{static_cast<value_type>(100)};    ///< Release (ms).
    value_type m_attackCoeff{static_cast<value_type>(0)};    ///< Coef. ataque.
    value_type m_releaseCoeff{static_cast<value_type>(0)};   ///< Coef. release.
    value_type m_state{static_cast<value_type>(0)};          ///< Estado e/m[n].
    value_type m_sampleRate{static_cast<value_type>(48000)}; ///< fs (Hz).
};

} // namespace cvdsp::dynamics

#endif // CVDSP_DYNAMICS_ENVELOPEFOLLOWER_HPP
