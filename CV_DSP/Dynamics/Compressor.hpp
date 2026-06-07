#ifndef CVDSP_DYNAMICS_COMPRESSOR_HPP
#define CVDSP_DYNAMICS_COMPRESSOR_HPP

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <algorithm>

#include "../Core/Types.hpp"
#include "EnvelopeFollower.hpp"

/**
 * @file Compressor.hpp
 * @brief Feed-forward dynamic range compressor for CV_DSP.
 * @namespace cvdsp::dynamics
 *
 * @section overview Visão geral
 *
 * Este módulo implementa um compressor de dinâmica *feed-forward* monoaural
 * com suporte a **soft knee** e **hard knee**, operando inteiramente no
 * domínio logarítmico (dB) para o cálculo de ganho e no domínio linear para
 * a aplicação de ganho. Reutiliza @ref EnvelopeFollower para a detecção do
 * nível de entrada (Peak ou RMS).
 *
 * Segue as regras do CV_DSP: header-only, C++20, sem dependências externas,
 * real-time safe (sem alocação, exceções ou RTTI em `process()`),
 * `template<typename T>` para `float` e `double`, com Doxygen completo.
 *
 * @section architecture Arquitetura do compressor feed-forward
 *
 * @verbatim
 *
 *   x[n] ──┬──────────────────────────────────────── × ──── y[n]
 *           │                                         ↑
 *           │   ┌─────────────┐   ┌──────────────┐   │
 *           └──▶│  Envelope   │──▶│    Gain      │──▶┘
 *               │  Follower   │   │   Computer   │   (linear gain)
 *               │(Peak / RMS) │   │(log domain)  │
 *               └─────────────┘   └──────────────┘
 *                                  threshold, ratio,
 *                                  knee, makeup
 *
 * @endverbatim
 *
 * Fluxo por amostra:
 * 1. O @b EnvelopeFollower estima o nível @f$ e[n] @f$ (linear, ≥ 0).
 * 2. O @b Gain @b Computer converte para dB, aplica a curva de compressão
 *    (threshold / ratio / knee) e devolve a redução de ganho em dB.
 * 3. A redução mais o makeup são convertidos para ganho linear e aplicados
 *    diretamente à amostra de entrada.
 *
 * @section gaincomp Gain Computer (computador de ganho)
 *
 * Opera inteiramente em dB. Seja @f$ X_{dB} @f$ o nível de entrada em dB
 * (obtido do envelope):
 *
 * @subsection hardknee Hard Knee
 *
 * @f[
 *     Y_{dB} =
 *     \begin{cases}
 *        X_{dB}, & X_{dB} < T \\[4pt]
 *        T + \dfrac{X_{dB} - T}{R}, & X_{dB} \ge T
 *     \end{cases}
 * @f]
 *
 * onde @f$ T @f$ é o threshold e @f$ R @f$ é o ratio.
 *
 * @subsection softknee Soft Knee
 *
 * Para uma largura de knee @f$ W @f$ (dB), a transição ocorre suavemente
 * entre @f$ T - W/2 @f$ e @f$ T + W/2 @f$:
 *
 * @f[
 *     Y_{dB} =
 *     \begin{cases}
 *        X_{dB}, & X_{dB} < T - W/2 \\[6pt]
 *        X_{dB} + \dfrac{(1/R - 1)\,(X_{dB} - T + W/2)^2}{2W},
 *                & T - W/2 \le X_{dB} \le T + W/2 \\[6pt]
 *        T + \dfrac{X_{dB} - T}{R}, & X_{dB} > T + W/2
 *     \end{cases}
 * @f]
 *
 * Nota: quando @f$ W = 0 @f$, o comportamento se reduz ao hard knee exato.
 *
 * @section gr Gain Reduction
 *
 * A redução de ganho em dB é simplesmente:
 *
 * @f[
 *     GR_{dB} = Y_{dB} - X_{dB} \;\le\; 0
 * @f]
 *
 * e o ganho linear aplicado à amostra é:
 *
 * @f[
 *     g = 10^{\,(GR_{dB} + \text{makeupDB})\,/\,20}
 * @f]
 *
 * @section envelopes Envelope Detection
 *
 * A detecção do nível é delegada ao módulo @ref EnvelopeFollower (com
 * ataque/release assimétricos). Isso desacopla a estimativa de amplitude da
 * curva de ganho, permitindo uso com detector Peak (reage a picos/transientes)
 * ou RMS (compressão mais "musical" baseada em energia percebida).
 *
 * @section utilities Funções utilitárias
 *
 * - @ref dbToGain : converte @f$ dB \to @f$ ganho linear @f$ = 10^{dB/20} @f$.
 * - @ref gainToDb : converte ganho linear @f$ \to @f$ dB @f$ = 20\log_{10}(g) @f$.
 *
 * @section rt Garantias de tempo real
 *
 * - `process()` é O(1), `noexcept`, sem alocação/exceções/RTTI.
 * - Usa `std::log10` e `std::pow` (ou equivalente `exp`/`log`) por amostra
 *   para as conversões linear↔dB. Para contextos SIMD-críticos recomenda-se
 *   usar lookup tables ou aproximações polinomiais (ex.: FastMath.hpp).
 * - Proteção contra nível zero (clampa para um piso antes de log).
 */
namespace cvdsp::dynamics
{

/**
 * @brief Converte decibéis em ganho linear.
 *
 * @f$ g = 10^{dB/20} @f$.
 *
 * @tparam T Tipo de ponto flutuante.
 * @param dB Nível em decibéis.
 * @return Ganho linear correspondente.
 */
template <typename T>
[[nodiscard]] inline T dbToGain(T dB) noexcept
{
    return std::pow(static_cast<T>(10), dB / static_cast<T>(20));
}

/**
 * @brief Converte ganho linear em decibéis.
 *
 * @f$ dB = 20\,\log_{10}(g) @f$.
 *
 * @tparam T Tipo de ponto flutuante.
 * @param gain Ganho linear (> 0; valores <= 0 produzem resultado indefinido).
 * @return Nível em dB.
 */
template <typename T>
[[nodiscard]] inline T gainToDb(T gain) noexcept
{
    return static_cast<T>(20) * std::log10(gain);
}

/**
 * @class Compressor
 * @brief Compressor feed-forward com soft/hard knee e makeup gain.
 *
 * @tparam T Tipo de ponto flutuante do processamento (`float` ou `double`).
 *
 * @par Exemplo — compressor vocal leve (soft knee):
 * @code
 *   cvdsp::dynamics::Compressor<float> comp;
 *   comp.prepare(48000.0f);
 *   comp.setThresholdDB(-18.0f);
 *   comp.setRatio(3.0f);
 *   comp.setAttackMs(10.0f);
 *   comp.setReleaseMs(100.0f);
 *   comp.setKneeDB(6.0f);     // soft knee de 6 dB
 *   comp.setMakeupGainDB(4.0f);
 *   for (std::size_t n = 0; n < numSamples; ++n)
 *       buffer[n] = comp.process(buffer[n]);
 * @endcode
 *
 * @par Exemplo — limiter agressivo (hard knee, ratio alto):
 * @code
 *   cvdsp::dynamics::Compressor<double> lim;
 *   lim.prepare(96000.0);
 *   lim.setThresholdDB(-1.0);
 *   lim.setRatio(100.0);      // ratio muito alto → quase limiter
 *   lim.setAttackMs(0.1);
 *   lim.setReleaseMs(50.0);
 *   lim.setKneeDB(0.0);       // hard knee (sem suavização)
 *   lim.setMakeupGainDB(0.0);
 *   double y = lim.process(x);
 * @endcode
 *
 * @par Exemplo — compressor RMS para bus (gentil):
 * @code
 *   cvdsp::dynamics::Compressor<float> bus;
 *   bus.prepare(48000.0f);
 *   bus.setDetectionMode(cvdsp::dynamics::EnvelopeMode::RMS);
 *   bus.setThresholdDB(-12.0f);
 *   bus.setRatio(2.0f);
 *   bus.setAttackMs(30.0f);
 *   bus.setReleaseMs(200.0f);
 *   bus.setKneeDB(10.0f);
 *   bus.setMakeupGainDB(3.0f);
 *   float y = bus.process(x);
 * @endcode
 */
template <typename T>
class Compressor
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "Compressor requires a floating point type (float or double)");

    /// @brief Tipo escalar de ponto flutuante usado pelo compressor.
    using value_type = T;
    /// @brief Tipo inteiro sem sinal para contagens e índices.
    using size_type = std::size_t;

    /**
     * @brief Constrói um compressor com parâmetros padrão estáveis.
     *
     * Padrões: threshold 0 dB (sem compressão), ratio 1:1 (unity), attack 10 ms,
     * release 100 ms, knee 0 dB (hard), makeup 0 dB. Não aloca.
     */
    constexpr Compressor() noexcept = default;

    /// @brief Destrutor trivial; nada é alocado.
    ~Compressor() noexcept = default;

    Compressor(const Compressor&) noexcept = default;
    Compressor& operator=(const Compressor&) noexcept = default;
    Compressor(Compressor&&) noexcept = default;
    Compressor& operator=(Compressor&&) noexcept = default;

    /**
     * @brief Prepara o compressor para uma taxa de amostragem.
     *
     * Deve ser chamado fora do audio thread. Delega ao EnvelopeFollower e
     * zera o estado de redução de ganho.
     *
     * @param sampleRate Taxa de amostragem em Hz (> 0).
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_envelope.prepare(sampleRate);
        m_currentGainReductionDB = static_cast<value_type>(0);
    }

    /**
     * @brief Zera o estado interno (envelope + gain reduction).
     *
     * Real-time safe: O(1), sem alocação/exceções.
     */
    inline void reset() noexcept
    {
        m_envelope.reset();
        m_currentGainReductionDB = static_cast<value_type>(0);
    }

    /**
     * @brief Define o threshold em dB (nível acima do qual ocorre compressão).
     * @param thresholdDB Threshold em dBFS (tipicamente negativo).
     */
    inline void setThresholdDB(value_type thresholdDB) noexcept
    {
        m_thresholdDB = thresholdDB;
    }

    /**
     * @brief Define o ratio de compressão (N:1).
     *
     * Ratio = 1 → sem compressão. Ratio = ∞ (ou valor muito grande) → limiter.
     * @param ratio Ratio (>= 1; clamped para min 1).
     */
    inline void setRatio(value_type ratio) noexcept
    {
        m_ratio = std::max(ratio, static_cast<value_type>(1));
    }

    /**
     * @brief Define o tempo de ataque do detector em milissegundos.
     * @param attackMs Tempo de ataque (ms, >= 0).
     */
    inline void setAttackMs(value_type attackMs) noexcept
    {
        m_envelope.setAttackMs(attackMs);
    }

    /**
     * @brief Define o tempo de release do detector em milissegundos.
     * @param releaseMs Tempo de release (ms, >= 0).
     */
    inline void setReleaseMs(value_type releaseMs) noexcept
    {
        m_envelope.setReleaseMs(releaseMs);
    }

    /**
     * @brief Define a largura do knee em dB.
     *
     * @f$ W = 0 @f$ → hard knee (transição abrupta);
     * @f$ W > 0 @f$ → soft knee (transição quadrática suave).
     * @param kneeDB Largura do knee em dB (>= 0; clamped para min 0).
     */
    inline void setKneeDB(value_type kneeDB) noexcept
    {
        m_kneeDB = std::max(kneeDB, static_cast<value_type>(0));
    }

    /**
     * @brief Define o ganho de compensação (makeup) em dB.
     *
     * Adicionado após a redução para compensar a perda de volume.
     * @param makeupDB Ganho de makeup em dB.
     */
    inline void setMakeupGainDB(value_type makeupDB) noexcept
    {
        m_makeupGainDB = makeupDB;
    }

    /**
     * @brief Define o modo de detecção do envelope (Peak ou RMS).
     *
     * Peak reage a transientes individuais; RMS comprime baseado em energia
     * percebida (mais "musical" para bus/master). Padrão: Peak.
     * @param mode Modo de detecção (ver @ref EnvelopeMode).
     */
    inline void setDetectionMode(EnvelopeMode mode) noexcept
    {
        m_envelope.setMode(mode);
    }

    /**
     * @brief Processa uma amostra e retorna o sinal comprimido.
     *
     * Fluxo:
     * 1. EnvelopeFollower → nível linear @f$ e @f$.
     * 2. @f$ X_{dB} = 20\log_{10}(\max(e, \epsilon)) @f$.
     * 3. Gain Computer → @f$ Y_{dB} @f$ (com knee).
     * 4. @f$ GR_{dB} = Y_{dB} - X_{dB} @f$.
     * 5. @f$ g = 10^{(GR_{dB} + \text{makeup})/20} @f$.
     * 6. @f$ y = x \cdot g @f$.
     *
     * @param x Amostra de entrada @f$ x[n] @f$.
     * @return Amostra comprimida @f$ y[n] @f$.
     *
     * Real-time safe: O(1), `noexcept`, sem alocação/exceções/RTTI.
     */
    [[nodiscard]] inline value_type process(value_type x) noexcept
    {
        const value_type twenty = static_cast<value_type>(20);

        // 1. Detecção de envelope (linear, >= 0).
        const value_type env = m_envelope.process(x);

        // 2. Converter envelope para dB (com piso para evitar log(0)).
        const value_type envClamped = std::max(env, kFloorLinear);
        const value_type inputDB = twenty * std::log10(envClamped);

        // 3. Gain Computer — calcula nível de saída desejado em dB.
        const value_type outputDB = computeGain(inputDB);

        // 4. Redução de ganho em dB (sempre <= 0 para compressão).
        const value_type grDB = outputDB - inputDB;
        m_currentGainReductionDB = grDB;

        // 5. Converter redução + makeup para ganho linear.
        const value_type totalDB = grDB + m_makeupGainDB;
        const value_type gainLin =
            std::pow(static_cast<value_type>(10), totalDB / twenty);

        // 6. Aplicar ganho à amostra original.
        return x * gainLin;
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

    /**
     * @brief Retorna a redução de ganho atual em dB (<= 0).
     *
     * Útil para medidores de GR em UIs.
     */
    [[nodiscard]] constexpr value_type getGainReductionDB() const noexcept
    {
        return m_currentGainReductionDB;
    }

    /// @brief Threshold configurado (dB).
    [[nodiscard]] constexpr value_type getThresholdDB() const noexcept { return m_thresholdDB; }
    /// @brief Ratio configurado (N:1).
    [[nodiscard]] constexpr value_type getRatio() const noexcept { return m_ratio; }
    /// @brief Knee configurado (dB).
    [[nodiscard]] constexpr value_type getKneeDB() const noexcept { return m_kneeDB; }
    /// @brief Makeup gain configurado (dB).
    [[nodiscard]] constexpr value_type getMakeupGainDB() const noexcept { return m_makeupGainDB; }

private:
    /**
     * @brief Gain Computer com suporte a soft knee.
     *
     * Dado @f$ X_{dB} @f$, retorna @f$ Y_{dB} @f$ conforme a curva de
     * compressão (threshold, ratio, knee).
     *
     * @param inputDB Nível de entrada em dB.
     * @return Nível de saída em dB (nunca maior que inputDB para ratio >= 1).
     */
    [[nodiscard]] inline value_type computeGain(value_type inputDB) const noexcept
    {
        const value_type thresh = m_thresholdDB;
        const value_type R = m_ratio;
        const value_type W = m_kneeDB;
        const value_type half = static_cast<value_type>(0.5);
        const value_type two = static_cast<value_type>(2);

        if (W <= static_cast<value_type>(0))
        {
            // Hard knee.
            if (inputDB < thresh)
                return inputDB;
            return thresh + (inputDB - thresh) / R;
        }

        // Soft knee — três regiões.
        const value_type lowerBound = thresh - W * half;
        const value_type upperBound = thresh + W * half;

        if (inputDB < lowerBound)
        {
            // Abaixo do knee: sem compressão.
            return inputDB;
        }
        if (inputDB > upperBound)
        {
            // Acima do knee: compressão total.
            return thresh + (inputDB - thresh) / R;
        }

        // Dentro do knee: transição quadrática suave.
        // Y = X + (1/R - 1) * (X - thresh + W/2)^2 / (2W)
        const value_type diff = inputDB - thresh + W * half;
        const value_type slope = (static_cast<value_type>(1) / R)
                                 - static_cast<value_type>(1);
        return inputDB + slope * (diff * diff) / (two * W);
    }

    /**
     * @brief Piso linear mínimo para evitar log10(0) = -inf.
     *
     * Equivale a aproximadamente -200 dBFS, muito abaixo de qualquer sinal
     * audível; garante que std::log10 nunca receba zero ou negativo.
     */
    static constexpr value_type kFloorLinear =
        static_cast<value_type>(1e-10);

    EnvelopeFollower<value_type> m_envelope{};          ///< Detector de nível.
    value_type m_thresholdDB{static_cast<value_type>(0)};  ///< Threshold (dB).
    value_type m_ratio{static_cast<value_type>(1)};        ///< Ratio (N:1).
    value_type m_kneeDB{static_cast<value_type>(0)};       ///< Knee width (dB).
    value_type m_makeupGainDB{static_cast<value_type>(0)}; ///< Makeup (dB).
    value_type m_currentGainReductionDB{static_cast<value_type>(0)}; ///< GR atual.
};

} // namespace cvdsp::dynamics

#endif // CVDSP_DYNAMICS_COMPRESSOR_HPP
