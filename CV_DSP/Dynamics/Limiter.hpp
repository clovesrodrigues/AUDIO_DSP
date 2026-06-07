#ifndef CVDSP_DYNAMICS_LIMITER_HPP
#define CVDSP_DYNAMICS_LIMITER_HPP

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <algorithm>

#include "../Core/Types.hpp"
#include "EnvelopeFollower.hpp"

/**
 * @file Limiter.hpp
 * @brief Brickwall limiter (sem lookahead) para CV_DSP.
 * @namespace cvdsp::dynamics
 *
 * @section overview Visão geral
 *
 * Implementa um **brickwall limiter** que garante que **nenhum sample** na saída
 * exceda o threshold configurado em valor absoluto. Opera com **ataque
 * instantâneo** (zero) e **release ajustável**, reutilizando o módulo
 * @ref EnvelopeFollower (modo Peak) para estimativa de nível.
 *
 * Segue as regras do CV_DSP: header-only, C++20, sem dependências externas,
 * real-time safe (sem alocação, exceções ou RTTI em `process()`),
 * `template<typename T>` para `float` e `double`, com Doxygen completo.
 *
 * @section brickwall Brickwall Limiting
 *
 * Um "brickwall" limiter é o processador de dinâmica mais agressivo possível:
 * garante que a saída **nunca** exceda uma amplitude máxima (o ceiling/threshold).
 * Diferentemente de um compressor com ratio finito, o limiter efetivamente impõe
 * ratio infinito (∞:1) acima do threshold.
 *
 * O princípio é:
 *
 * @f[
 *     g[n] = \min\!\Big(1,\;\frac{T_{\text{lin}}}{e[n]}\Big)
 * @f]
 * @f[
 *     y[n] = x[n] \cdot g[n]
 * @f]
 *
 * onde @f$ T_{\text{lin}} = 10^{T_{dB}/20} @f$ é o threshold em escala linear e
 * @f$ e[n] @f$ é o envelope instantâneo de pico.
 *
 * @subsection guarantee Prova da garantia brickwall
 *
 * Usamos o @ref EnvelopeFollower com **ataque zero** (instantâneo). Isso garante:
 *
 * - **Caso ataque** (@f$ |x[n]| > e[n-1] @f$):
 *   @f$ e[n] = |x[n]| @f$ (snap instantâneo).
 *   @f$ g[n] = T_{\text{lin}} / |x[n]| @f$.
 *   @f$ |y[n]| = |x[n]| \cdot T_{\text{lin}}/|x[n]| = T_{\text{lin}} @f$. ✓
 *
 * - **Caso release** (@f$ |x[n]| \le e[n-1] @f$):
 *   @f$ e[n] @f$ é uma média ponderada entre @f$ e[n-1] @f$ e @f$ |x[n]| @f$;
 *   como @f$ e[n-1] > |x[n]| @f$, segue que @f$ e[n] > |x[n]| @f$.
 *   Se @f$ e[n] > T_{\text{lin}} @f$:
 *   @f$ g[n] = T_{\text{lin}}/e[n] @f$,
 *   @f$ |y[n]| = |x[n]| \cdot T_{\text{lin}}/e[n] < T_{\text{lin}} @f$. ✓
 *   Se @f$ e[n] \le T_{\text{lin}} @f$: @f$ g[n] = 1 @f$,
 *   @f$ |y[n]| = |x[n]| \le e[n] \le T_{\text{lin}} @f$. ✓
 *
 * Portanto **nenhum sample excede** @f$ T_{\text{lin}} @f$ na saída.
 *
 * @section lookahead Lookahead Limiting (informação teórica)
 *
 * Limiters profissionais (ex.: ISP, true-peak limiters) utilizam um buffer de
 * lookahead para "ver o futuro" e iniciar a redução de ganho @b antes do
 * transiente chegar:
 *
 * @verbatim
 *   tempo ────────────────────▶
 *         ┌── lookahead ──┐
 *         │  gain começa a│
 *         │  cair aqui    │  pico real
 *         ▼               ▼
 *   ......─────╲         ╱───────
 *               ╲       ╱
 *                ╲_____╱   ← gain reduzido suavemente
 * @endverbatim
 *
 * Vantagens do lookahead:
 * - Elimina o "click" / distorção do ataque instantâneo pois a rampa de ganho
 *   é suave.
 * - Permite atacar verdadeiros picos inter-amostra (true-peak) quando combinado
 *   com oversampling.
 *
 * Desvantagens:
 * - Introduz latência adicional (tipicamente 1–5 ms).
 * - Requer um buffer circular (alocação prévia em `prepare()`).
 *
 * Esta implementação **não usa lookahead** (zero latência), trocando suavidade
 * do ataque por latência zero e simplicidade. Para um limiter true-peak com
 * lookahead, recomenda-se estender esta classe com um `DelayLine` e uma rampa
 * de gain pré-computada.
 *
 * @section truepeak True Peak (informação teórica)
 *
 * Conversores D/A reconstroem o sinal contínuo entre amostras (interpolação
 * sinc). Picos **inter-amostra** (entre pontos discretos) podem exceder o
 * valor máximo dos samples:
 *
 * @verbatim
 *      samples:   0.9   1.0   0.9
 *      contínuo:     ╱╲
 *                   /  \     ← true-peak pode ser ~1.05 (> 1.0)
 *                  /    \
 * @endverbatim
 *
 * Para detectar true-peaks, o standard ITU-R BS.1770 especifica oversampling
 * (tipicamente 4×) com filtro FIR de reconstrução antes da detecção de pico.
 *
 * Esta implementação opera apenas em **sample-peak** (cada amostra discreta).
 * Ela garante que nenhum sample digital exceda o threshold; true-peaks
 * inter-amostra podem ainda existir ~0.5–3 dB acima. Para proteção contra
 * true-peak, aplicar oversampling (ex.: 4×) antes deste limiter ou usar um
 * limiter com lookahead + oversampled peak detection.
 *
 * @section rt Garantias de tempo real
 *
 * - `process()` é O(1), `noexcept`, sem alocação/exceções/RTTI.
 * - Zero latência (sem lookahead buffer).
 * - Nenhum sample na saída excede o threshold (garantia brickwall).
 * - Proteção contra denormais no estado do envelope.
 */
namespace cvdsp::dynamics
{

/**
 * @class Limiter
 * @brief Brickwall limiter sem lookahead com ataque instantâneo.
 *
 * @tparam T Tipo de ponto flutuante do processamento (`float` ou `double`).
 *
 * @par Exemplo — limiter de saída (ceiling -0.3 dBFS):
 * @code
 *   cvdsp::dynamics::Limiter<float> lim;
 *   lim.prepare(48000.0f);
 *   lim.setThresholdDB(-0.3f);
 *   lim.setReleaseMs(50.0f);
 *   for (std::size_t n = 0; n < numSamples; ++n)
 *       buffer[n] = lim.process(buffer[n]);
 *   // Garantia: |buffer[n]| <= 10^(-0.3/20) ≈ 0.966 para todo n.
 * @endcode
 *
 * @par Exemplo — safety limiter antes do DAC:
 * @code
 *   cvdsp::dynamics::Limiter<double> safety;
 *   safety.prepare(96000.0);
 *   safety.setThresholdDB(0.0);   // ceiling = 1.0 (0 dBFS)
 *   safety.setReleaseMs(100.0);
 *   double y = safety.process(x); // |y| <= 1.0 sempre
 * @endcode
 *
 * @par Exemplo — uso em cadeia (após compressor):
 * @code
 *   // Compressor reduz a dinâmica; limiter garante ceiling absoluto.
 *   float y = comp.process(x);       // pode ter overshoot residual
 *   y = limiter.process(y);          // brickwall garante |y| <= threshold
 * @endcode
 */
template <typename T>
class Limiter
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "Limiter requires a floating point type (float or double)");

    /// @brief Tipo escalar de ponto flutuante usado pelo limiter.
    using value_type = T;
    /// @brief Tipo inteiro sem sinal para contagens e índices.
    using size_type = std::size_t;

    /**
     * @brief Constrói um limiter com threshold 0 dBFS (unity) e release 50 ms.
     *
     * Estado zerado; coeficientes válidos após @ref prepare. Não aloca.
     */
    constexpr Limiter() noexcept = default;

    /// @brief Destrutor trivial; nada é alocado.
    ~Limiter() noexcept = default;

    Limiter(const Limiter&) noexcept = default;
    Limiter& operator=(const Limiter&) noexcept = default;
    Limiter(Limiter&&) noexcept = default;
    Limiter& operator=(Limiter&&) noexcept = default;

    /**
     * @brief Prepara o limiter para uma taxa de amostragem.
     *
     * Deve ser chamado fora do audio thread. Configura o EnvelopeFollower
     * interno (Peak, attack=0, release conforme setReleaseMs), zera estado e
     * recalcula o threshold linear.
     *
     * @param sampleRate Taxa de amostragem em Hz (> 0).
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);
        m_envelope.prepare(m_sampleRate);
        m_envelope.setMode(EnvelopeMode::Peak);
        m_envelope.setAttackMs(static_cast<value_type>(0)); // instantâneo
        m_envelope.setReleaseMs(m_releaseMs);
        updateThresholdLinear();
    }

    /**
     * @brief Zera o estado interno do limiter.
     *
     * Real-time safe: O(1), sem alocação/exceções.
     */
    inline void reset() noexcept
    {
        m_envelope.reset();
    }

    /**
     * @brief Define o threshold (ceiling) em dBFS.
     *
     * Nenhum sample de saída terá valor absoluto acima de
     * @f$ 10^{T_{dB}/20} @f$. Tipicamente 0 dB (unity) ou ligeiramente
     * negativo (-0.1 a -1 dBFS) para headroom em conversores.
     *
     * @param thresholdDB Threshold em dBFS (<= 0 para não amplificar).
     */
    inline void setThresholdDB(value_type thresholdDB) noexcept
    {
        m_thresholdDB = thresholdDB;
        updateThresholdLinear();
    }

    /**
     * @brief Define o tempo de release em milissegundos.
     *
     * Controla quão rápido o ganho retorna a 1.0 após um transiente ser
     * limitado. Valores menores = release mais rápido (mais distorção);
     * maiores = release mais suave (mais bombeamento/ducking).
     *
     * @param releaseMs Tempo de release (ms, >= 0; 0 = instantâneo).
     */
    inline void setReleaseMs(value_type releaseMs) noexcept
    {
        m_releaseMs = std::max(releaseMs, static_cast<value_type>(0));
        m_envelope.setReleaseMs(m_releaseMs);
    }

    /**
     * @brief Processa uma amostra e retorna o sinal limitado.
     *
     * Garante @f$ |y[n]| \le T_{\text{lin}} @f$ para toda amostra.
     *
     * Fluxo:
     * 1. EnvelopeFollower(Peak, attack=0) → @f$ e[n] \ge |x[n]| @f$ (provado).
     * 2. @f$ g[n] = \min(1,\; T_{\text{lin}} / \max(e[n], \epsilon)) @f$.
     * 3. @f$ y[n] = x[n] \cdot g[n] @f$.
     *
     * @param x Amostra de entrada @f$ x[n] @f$.
     * @return Amostra limitada @f$ y[n] @f$ com @f$ |y[n]| \le T_{\text{lin}} @f$.
     *
     * Real-time safe: O(1), `noexcept`, sem alocação/exceções/RTTI.
     */
    [[nodiscard]] inline value_type process(value_type x) noexcept
    {
        // 1. Envelope instantâneo de pico (env >= |x| garantido).
        const value_type env = m_envelope.process(x);

        // 2. Ganho necessário para não exceder threshold.
        const value_type envSafe = std::max(env, kFloorLinear);
        const value_type gain = std::min(static_cast<value_type>(1),
                                         m_thresholdLinear / envSafe);

        // 3. Aplicar ganho.
        return x * gain;
    }

    /**
     * @brief Processa um bloco de amostras in-place.
     * @param buffer Ponteiro para as amostras (não-nulo se @p numSamples > 0).
     * @param numSamples Quantidade de amostras a processar.
     *
     * Real-time safe: O(N), sem alocação/exceções.
     */
    inline void processBlock(value_type* buffer, size_type numSamples) noexcept
    {
        for (size_type n = 0; n < numSamples; ++n)
            buffer[n] = process(buffer[n]);
    }

    /// @brief Threshold configurado (dBFS).
    [[nodiscard]] constexpr value_type getThresholdDB() const noexcept { return m_thresholdDB; }
    /// @brief Threshold linear em uso.
    [[nodiscard]] constexpr value_type getThresholdLinear() const noexcept { return m_thresholdLinear; }
    /// @brief Tempo de release configurado (ms).
    [[nodiscard]] constexpr value_type getReleaseMs() const noexcept { return m_releaseMs; }
    /// @brief Taxa de amostragem (Hz).
    [[nodiscard]] constexpr value_type getSampleRate() const noexcept { return m_sampleRate; }

    /**
     * @brief Retorna a redução de ganho atual em dB (<= 0).
     *
     * Calculada a partir do último envelope/threshold: quanto o limiter está
     * "segurando" o sinal neste instante.
     */
    [[nodiscard]] inline value_type getGainReductionDB() const noexcept
    {
        const value_type env = m_envelope.getEnvelope();
        if (env <= m_thresholdLinear)
            return static_cast<value_type>(0);
        // GR = 20*log10(threshold/env)
        return static_cast<value_type>(20) *
               std::log10(m_thresholdLinear / std::max(env, kFloorLinear));
    }

private:
    /**
     * @brief Recalcula o threshold linear a partir de m_thresholdDB.
     */
    inline void updateThresholdLinear() noexcept
    {
        m_thresholdLinear =
            std::pow(static_cast<value_type>(10),
                     m_thresholdDB / static_cast<value_type>(20));
    }

    /// @brief Piso linear para evitar divisão por zero.
    static constexpr value_type kFloorLinear =
        static_cast<value_type>(1e-10);

    EnvelopeFollower<value_type> m_envelope{};                ///< Detector Peak.
    value_type m_thresholdDB{static_cast<value_type>(0)};     ///< Threshold (dB).
    value_type m_thresholdLinear{static_cast<value_type>(1)}; ///< Threshold linear.
    value_type m_releaseMs{static_cast<value_type>(50)};      ///< Release (ms).
    value_type m_sampleRate{static_cast<value_type>(48000)};  ///< fs (Hz).
};

} // namespace cvdsp::dynamics

#endif // CVDSP_DYNAMICS_LIMITER_HPP
