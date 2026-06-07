#ifndef CVDSP_DYNAMICS_NOISEGATE_HPP
#define CVDSP_DYNAMICS_NOISEGATE_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <algorithm>

#include "../Core/Types.hpp"
#include "EnvelopeFollower.hpp"

/**
 * @file NoiseGate.hpp
 * @brief Noise gate com hysteresis, hold e ataque/release para CV_DSP.
 * @namespace cvdsp::dynamics
 *
 * @section overview Visão geral
 *
 * Um **noise gate** silencia o sinal quando ele cai abaixo de um limiar,
 * permitindo apenas passagem quando o sinal é "forte o suficiente". É
 * fundamental para instrumentos como guitarra elétrica, onde captadores de
 * alta impedância captam ruído eletromagnético constante que deve ser
 * eliminado nos silêncios.
 *
 * Esta implementação usa **dois limiares** (open / close) configurados
 * independentemente, criando uma banda de **hysteresis** que impede
 * oscilação rápida (chattering) ao redor de um único threshold.
 *
 * Segue as regras do CV_DSP: header-only, C++20, sem dependências externas,
 * real-time safe (sem alocação, exceções ou RTTI em `process()`),
 * `template<typename T>` para `float` e `double`, com Doxygen completo.
 *
 * @section guitar Gates para guitarra
 *
 * Em uma cadeia de efeitos de guitarra, o noise gate tipicamente fica
 * **antes** dos efeitos de distorção/overdrive ou como primeiro efeito:
 *
 * @verbatim
 *   Guitarra → [NoiseGate] → Overdrive → EQ → Amp
 * @endverbatim
 *
 * Por quê? A distorção amplifica o ruído de fundo dramaticamente. Um gate
 * antes da distorção elimina o ruído **antes** de ser amplificado, resultando
 * em silêncios limpos entre notas. Em setups de metal/high-gain, o gate é
 * essencial para "tight stops" (paradas bruscas entre riffs).
 *
 * Parâmetros típicos para guitarra:
 * - Threshold Open: −40 a −30 dBFS (acima do nível de ruído dos captadores)
 * - Threshold Close: 3–6 dB abaixo do Open (hysteresis)
 * - Attack: 0.1–2 ms (muito rápido para preservar transientes de palhetada)
 * - Hold: 20–100 ms (sustém notas curtas sem cortar)
 * - Release: 10–50 ms (decai suavemente sem "click" abrupto)
 *
 * @section pickups Ruído de captadores
 *
 * Captadores magnéticos de guitarra (single-coil e humbucker) são
 * susceptíveis a:
 * - **Hum 50/60 Hz**: interferência de rede elétrica (mais forte em
 *   single-coils).
 * - **Ruído térmico**: gerado pela resistência do fio do captador
 *   (~5–20 kΩ); aparece como um chiado (hiss) de banda larga.
 * - **EMI/RFI**: interferência de fontes externas (lâmpadas fluorescentes,
 *   dimmers, aparelhos digitais, Wi-Fi).
 *
 * Níveis típicos de ruído de captadores em silêncio:
 * - Single-coil: −50 a −40 dBFS (alto, especialmente hum)
 * - Humbucker: −60 a −50 dBFS (o modo diferencial cancela o hum)
 * - Captadores ativos (EMG, Fishman): −70 a −60 dBFS (pré-amp interno,
 *   baixa impedância)
 *
 * O noise gate deve ter threshold ajustado **acima** deste piso de ruído
 * para silenciar o sinal nos trechos sem toque.
 *
 * @section hysteresis Hysteresis
 *
 * Com um único threshold, o gate pode "chattering" (abrir/fechar
 * repetidamente) quando o sinal oscila ao redor do limiar. Hysteresis
 * resolve isso usando **dois** limiares distintos:
 *
 * @verbatim
 *   nível (dB)
 *    ^
 *    |     thresholdOpen ─────── abre aqui ──────▶ OPEN
 *    |  ╔═══════════════════╗
 *    |  ║   hysteresis band ║   gate permanece no estado atual
 *    |  ╚═══════════════════╝
 *    |     thresholdClose ────── fecha aqui ─────▶ CLOSED
 *    |
 *    └──────────────────────── tempo ──▶
 * @endverbatim
 *
 * - O gate **abre** quando o nível cruza @b acima de `thresholdOpen`.
 * - O gate **fecha** quando o nível cruza @b abaixo de `thresholdClose`.
 * - `thresholdClose < thresholdOpen` cria a banda de hysteresis.
 * - Quanto maior a diferença, mais estável o gate (menos chattering),
 *   mas menos sensível a sinais fracos.
 *
 * @section statemachine Máquina de estados
 *
 * @verbatim
 *
 *               env > threshOpen
 *    ┌────────┐ ──────────────▶ ┌────────┐
 *    │ CLOSED │                 │  OPEN  │
 *    │(gain→0)│ ◀────────────── │(gain→1)│
 *    └────────┘  holdTimer      └────────┘
 *        ▲       expirado           │
 *        │                          │ env < threshClose
 *        │       ┌─────────┐        │
 *        └────── │ HOLDING │ ◀──────┘
 *   holdTimer    │(gain=1) │   env > threshClose
 *   expirado     └─────────┘ ──────▶ volta a OPEN
 *
 * @endverbatim
 *
 * - **Closed**: ganho tende a 0 (rampa de release). Quando o nível excede
 *   `thresholdOpen`, transiciona para Open.
 * - **Open**: ganho tende a 1 (rampa de attack). Quando o nível cai abaixo
 *   de `thresholdClose`, transiciona para Holding.
 * - **Holding**: ganho permanece em 1 (sustenta). Um contador de hold
 *   conta regressivamente. Se o nível subir acima de `thresholdClose`,
 *   volta para Open. Se o contador expira, transiciona para Closed.
 *
 * @section gainramp Suavização de ganho
 *
 * O ganho aplicado ao sinal é suavizado com um filtro de um pólo
 * assimétrico:
 *
 * @f[
 *     g[n] =
 *     \begin{cases}
 *       \alpha_a\, g[n-1] + (1-\alpha_a)\cdot 1, & \text{abrindo}\\
 *       \alpha_r\, g[n-1] + (1-\alpha_r)\cdot 0, & \text{fechando}
 *     \end{cases}
 * @f]
 *
 * onde @f$ \alpha_a = e^{-1/(\tau_a f_s)} @f$ e
 * @f$ \alpha_r = e^{-1/(\tau_r f_s)} @f$.
 *
 * @section rt Garantias de tempo real
 *
 * - `process()` é O(1), `noexcept`, sem alocação/exceções/RTTI.
 * - Detecção de nível via @ref EnvelopeFollower (Peak, attack e release
 *   rápidos) para evitar chattering em amostras individuais.
 * - Zero latência (sem lookahead).
 */
namespace cvdsp::dynamics
{

/**
 * @brief Estado interno do noise gate.
 */
enum class GateState
{
    Closed,  ///< Gate fechado — ganho tendendo a 0 (silenciando).
    Open,    ///< Gate aberto — ganho tendendo a 1 (passando sinal).
    Holding  ///< Gate em hold — ganho em 1, aguardando timer expirar.
};

/**
 * @class NoiseGate
 * @brief Noise gate com hysteresis, hold e suavização de ataque/release.
 *
 * @tparam T Tipo de ponto flutuante do processamento (`float` ou `double`).
 *
 * @par Exemplo — gate para guitarra high-gain:
 * @code
 *   cvdsp::dynamics::NoiseGate<float> gate;
 *   gate.prepare(48000.0f);
 *   gate.setThresholdOpenDB(-35.0f);
 *   gate.setThresholdCloseDB(-40.0f);  // 5 dB hysteresis
 *   gate.setAttackMs(0.5f);
 *   gate.setHoldMs(50.0f);
 *   gate.setReleaseMs(30.0f);
 *   for (std::size_t n = 0; n < numSamples; ++n)
 *       buffer[n] = gate.process(buffer[n]);
 * @endcode
 *
 * @par Exemplo — gate suave para vocal (redução de bleed):
 * @code
 *   cvdsp::dynamics::NoiseGate<double> voxGate;
 *   voxGate.prepare(96000.0);
 *   voxGate.setThresholdOpenDB(-45.0);
 *   voxGate.setThresholdCloseDB(-50.0);
 *   voxGate.setAttackMs(2.0);
 *   voxGate.setHoldMs(100.0);
 *   voxGate.setReleaseMs(80.0);
 *   double y = voxGate.process(x);
 * @endcode
 *
 * @par Exemplo — gate em cadeia com compressor:
 * @code
 *   // Gate remove ruído, compressor controla dinâmica.
 *   float y = gate.process(guitarSample);   // silencia ruído
 *   y = compressor.process(y);              // comprime picos
 * @endcode
 */
template <typename T>
class NoiseGate
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "NoiseGate requires a floating point type (float or double)");

    /// @brief Tipo escalar de ponto flutuante.
    using value_type = T;
    /// @brief Tipo inteiro sem sinal para contagens e índices.
    using size_type = std::size_t;

    /**
     * @brief Constrói um noise gate com parâmetros padrão.
     *
     * Padrões: threshOpen -40 dB, threshClose -45 dB (5 dB hysteresis),
     * attack 1 ms, hold 50 ms, release 30 ms, estado Closed. Não aloca.
     */
    constexpr NoiseGate() noexcept = default;

    /// @brief Destrutor trivial; nada é alocado.
    ~NoiseGate() noexcept = default;

    NoiseGate(const NoiseGate&) noexcept = default;
    NoiseGate& operator=(const NoiseGate&) noexcept = default;
    NoiseGate(NoiseGate&&) noexcept = default;
    NoiseGate& operator=(NoiseGate&&) noexcept = default;

    /**
     * @brief Prepara o gate para uma taxa de amostragem.
     *
     * Deve ser chamado fora do audio thread. Configura o detector de nível
     * interno, recalcula coeficientes e limiares lineares.
     *
     * @param sampleRate Taxa de amostragem em Hz (> 0).
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);

        // Detector de nível: Peak com attack muito rápido e release curto
        // para detecção responsiva sem chattering.
        m_detector.prepare(m_sampleRate);
        m_detector.setMode(EnvelopeMode::Peak);
        m_detector.setAttackMs(static_cast<value_type>(0.05));
        m_detector.setReleaseMs(static_cast<value_type>(1));

        updateThresholdsLinear();
        updateAttackCoeff();
        updateReleaseCoeff();
        updateHoldSamples();

        reset();
    }

    /**
     * @brief Zera o estado interno do gate (fecha, zera ganho e detector).
     *
     * Real-time safe: O(1), sem alocação/exceções.
     */
    inline void reset() noexcept
    {
        m_detector.reset();
        m_state = GateState::Closed;
        m_gain = static_cast<value_type>(0);
        m_holdCounter = 0;
    }

    /**
     * @brief Define o threshold de abertura em dBFS.
     *
     * O gate abre quando o nível sobe acima deste limiar.
     * @param thresholdDB Threshold open (dB).
     */
    inline void setThresholdOpenDB(value_type thresholdDB) noexcept
    {
        m_thresholdOpenDB = thresholdDB;
        updateThresholdsLinear();
    }

    /**
     * @brief Define o threshold de fechamento em dBFS.
     *
     * O gate fecha (após hold) quando o nível cai abaixo deste limiar.
     * Deve ser <= thresholdOpen para hysteresis efetiva.
     * @param thresholdDB Threshold close (dB).
     */
    inline void setThresholdCloseDB(value_type thresholdDB) noexcept
    {
        m_thresholdCloseDB = thresholdDB;
        updateThresholdsLinear();
    }

    /**
     * @brief Define o tempo de ataque (abertura) em milissegundos.
     *
     * Controla quão rápido o ganho sobe de 0 a 1 quando o gate abre.
     * Valores muito altos podem cortar o transiente; muito baixos podem
     * causar click.
     * @param attackMs Tempo de ataque (ms, >= 0; 0 = instantâneo).
     */
    inline void setAttackMs(value_type attackMs) noexcept
    {
        m_attackMs = std::max(attackMs, static_cast<value_type>(0));
        updateAttackCoeff();
    }

    /**
     * @brief Define o tempo de release (fechamento) em milissegundos.
     *
     * Controla quão rápido o ganho cai de 1 a 0 quando o gate fecha
     * (após o hold expirar).
     * @param releaseMs Tempo de release (ms, >= 0; 0 = instantâneo).
     */
    inline void setReleaseMs(value_type releaseMs) noexcept
    {
        m_releaseMs = std::max(releaseMs, static_cast<value_type>(0));
        updateReleaseCoeff();
    }

    /**
     * @brief Define o tempo de hold em milissegundos.
     *
     * O gate permanece aberto por este período após o nível cair abaixo
     * de thresholdClose, antes de iniciar o release. Previne cortes
     * prematuros de notas sustentadas.
     * @param holdMs Tempo de hold (ms, >= 0).
     */
    inline void setHoldMs(value_type holdMs) noexcept
    {
        m_holdMs = std::max(holdMs, static_cast<value_type>(0));
        updateHoldSamples();
    }

    /**
     * @brief Processa uma amostra e retorna o sinal com gate aplicado.
     *
     * Fluxo:
     * 1. EnvelopeFollower(Peak) → nível suavizado.
     * 2. Máquina de estados (Closed/Open/Holding) com hysteresis.
     * 3. Suavização de ganho (ataque/release one-pole).
     * 4. Saída = entrada × ganho.
     *
     * @param x Amostra de entrada @f$ x[n] @f$.
     * @return Amostra com gate @f$ y[n] = x[n] \cdot g[n] @f$.
     *
     * Real-time safe: O(1), `noexcept`, sem alocação/exceções/RTTI.
     */
    [[nodiscard]] inline value_type process(value_type x) noexcept
    {
        const value_type one = static_cast<value_type>(1);
        const value_type zero = static_cast<value_type>(0);

        // 1. Detecção de nível suavizada.
        const value_type env = m_detector.process(x);

        // 2. Máquina de estados com hysteresis.
        switch (m_state)
        {
        case GateState::Closed:
            if (env > m_thresholdOpenLinear)
                m_state = GateState::Open;
            break;

        case GateState::Open:
            if (env < m_thresholdCloseLinear)
            {
                m_state = GateState::Holding;
                m_holdCounter = m_holdSamples;
            }
            break;

        case GateState::Holding:
            if (env > m_thresholdCloseLinear)
            {
                // Re-abre: sinal voltou acima do threshold de fechamento.
                m_state = GateState::Open;
            }
            else if (m_holdCounter > 0)
            {
                --m_holdCounter;
            }
            else
            {
                // Hold expirado: fecha o gate.
                m_state = GateState::Closed;
            }
            break;
        }

        // 3. Ganho-alvo e suavização.
        const value_type targetGain = (m_state == GateState::Closed) ? zero : one;

        if (targetGain > m_gain)
        {
            // Abrindo: rampa de ataque.
            m_gain = m_attackCoeff * m_gain +
                     (one - m_attackCoeff) * targetGain;
        }
        else
        {
            // Fechando (ou já fechado): rampa de release.
            m_gain = m_releaseCoeff * m_gain +
                     (one - m_releaseCoeff) * targetGain;
        }

        // Proteção contra denormais.
        m_gain += kDenormalGuard;
        m_gain -= kDenormalGuard;

        // 4. Aplicar ganho.
        return x * m_gain;
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

    /// @brief Estado atual do gate.
    [[nodiscard]] constexpr GateState getState() const noexcept { return m_state; }
    /// @brief Ganho atual aplicado (0..1 linear).
    [[nodiscard]] constexpr value_type getGain() const noexcept { return m_gain; }
    /// @brief Threshold open (dB).
    [[nodiscard]] constexpr value_type getThresholdOpenDB() const noexcept { return m_thresholdOpenDB; }
    /// @brief Threshold close (dB).
    [[nodiscard]] constexpr value_type getThresholdCloseDB() const noexcept { return m_thresholdCloseDB; }
    /// @brief Tempo de ataque (ms).
    [[nodiscard]] constexpr value_type getAttackMs() const noexcept { return m_attackMs; }
    /// @brief Tempo de release (ms).
    [[nodiscard]] constexpr value_type getReleaseMs() const noexcept { return m_releaseMs; }
    /// @brief Tempo de hold (ms).
    [[nodiscard]] constexpr value_type getHoldMs() const noexcept { return m_holdMs; }
    /// @brief Taxa de amostragem (Hz).
    [[nodiscard]] constexpr value_type getSampleRate() const noexcept { return m_sampleRate; }

private:
    /**
     * @brief Converte um tempo (ms) em coeficiente de um pólo.
     */
    [[nodiscard]] inline value_type coeffFromMs(value_type timeMs) const noexcept
    {
        if (timeMs <= static_cast<value_type>(0))
            return static_cast<value_type>(0);
        const value_type tauSamples =
            (timeMs * static_cast<value_type>(0.001)) * m_sampleRate;
        return std::exp(static_cast<value_type>(-1) / tauSamples);
    }

    inline void updateThresholdsLinear() noexcept
    {
        m_thresholdOpenLinear =
            std::pow(static_cast<value_type>(10),
                     m_thresholdOpenDB / static_cast<value_type>(20));
        m_thresholdCloseLinear =
            std::pow(static_cast<value_type>(10),
                     m_thresholdCloseDB / static_cast<value_type>(20));
    }

    inline void updateAttackCoeff() noexcept
    {
        m_attackCoeff = coeffFromMs(m_attackMs);
    }

    inline void updateReleaseCoeff() noexcept
    {
        m_releaseCoeff = coeffFromMs(m_releaseMs);
    }

    inline void updateHoldSamples() noexcept
    {
        m_holdSamples = static_cast<std::int32_t>(
            m_holdMs * static_cast<value_type>(0.001) * m_sampleRate +
            static_cast<value_type>(0.5));
    }

    /// @brief Constante anti-denormal.
    static constexpr value_type kDenormalGuard =
        static_cast<value_type>(1e-20);

    EnvelopeFollower<value_type> m_detector{};                       ///< Detector.
    value_type m_thresholdOpenDB{static_cast<value_type>(-40)};      ///< Open (dB).
    value_type m_thresholdCloseDB{static_cast<value_type>(-45)};     ///< Close (dB).
    value_type m_thresholdOpenLinear{static_cast<value_type>(0.01)}; ///< Open (lin).
    value_type m_thresholdCloseLinear{static_cast<value_type>(0.0056234)}; ///< Close (lin).
    value_type m_attackMs{static_cast<value_type>(1)};               ///< Attack (ms).
    value_type m_releaseMs{static_cast<value_type>(30)};             ///< Release (ms).
    value_type m_holdMs{static_cast<value_type>(50)};                ///< Hold (ms).
    value_type m_attackCoeff{static_cast<value_type>(0)};            ///< Coef. attack.
    value_type m_releaseCoeff{static_cast<value_type>(0)};           ///< Coef. release.
    std::int32_t m_holdSamples{0};                                   ///< Hold (samples).
    std::int32_t m_holdCounter{0};                                   ///< Contador hold.
    GateState m_state{GateState::Closed};                            ///< Estado atual.
    value_type m_gain{static_cast<value_type>(0)};                   ///< Ganho (0..1).
    value_type m_sampleRate{static_cast<value_type>(48000)};         ///< fs (Hz).
};

} // namespace cvdsp::dynamics

#endif // CVDSP_DYNAMICS_NOISEGATE_HPP
