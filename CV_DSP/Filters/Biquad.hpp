#ifndef CVDSP_FILTERS_BIQUAD_HPP
#define CVDSP_FILTERS_BIQUAD_HPP

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <algorithm>

#include "../Core/Types.hpp"
#include "../Core/Constants.hpp"

/**
 * @file Biquad.hpp
 * @brief Second-order (biquad) IIR filter — RBJ Audio EQ Cookbook, Direct Form
 *        II Transposed — for CV_DSP.
 * @namespace cvdsp::filters
 *
 * @section overview Visão geral
 *
 * Este módulo implementa um filtro biquad (IIR de 2ª ordem) configurável,
 * cobrindo os oito tipos clássicos do *Audio EQ Cookbook* de Robert
 * Bristow-Johnson (RBJ): passa-baixas, passa-altas, passa-banda, rejeita-banda
 * (notch), passa-tudo, peaking EQ e shelving (low/high). O processamento usa a
 * estrutura **Direct Form II Transposed (DF2T)**, que oferece excelente
 * comportamento numérico em ponto flutuante.
 *
 * Projetado conforme as regras do CV_DSP: header-only, C++20, sem dependências
 * externas, real-time safe (sem alocação, exceções ou RTTI em `process()`),
 * `template<typename T>` para `float` e `double`, com Doxygen completo.
 *
 * @section diffeq Equação geral do biquad
 *
 * Um biquad realiza a equação de diferenças (coeficientes já normalizados por
 * @f$ a_0 @f$):
 *
 * @f[
 *     y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2]
 *                     - a_1 y[n-1] - a_2 y[n-2]
 * @f]
 *
 * cuja função de transferência é:
 *
 * @f[
 *     H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}
 *                 {1   + a_1 z^{-1} + a_2 z^{-2}}
 * @f]
 *
 * @section df2t Direct Form II Transposed
 *
 * A forma direta II transposta calcula a mesma @f$ H(z) @f$ usando apenas dois
 * estados (@f$ s_1, s_2 @f$) e a seguinte recorrência por amostra:
 *
 * @f[
 *   \begin{aligned}
 *     y[n] &= b_0\,x[n] + s_1 \\
 *     s_1  &= b_1\,x[n] - a_1\,y[n] + s_2 \\
 *     s_2  &= b_2\,x[n] - a_2\,y[n]
 *   \end{aligned}
 * @f]
 *
 * Vantagens da DF2T frente à Direct Form I/II:
 * - Usa o mínimo de elementos de atraso (2 estados).
 * - É numericamente robusta com coeficientes em ponto flutuante: o erro de
 *   quantização/arredondamento é moldado favoravelmente, reduzindo ruído e
 *   risco de oscilações de ciclo-limite em precisão simples (`float`).
 * - Mantém os estados em escala compatível com a entrada, evitando overflow
 *   interno em estágios de alto Q.
 *
 * @section design Projeto dos coeficientes (RBJ Cookbook)
 *
 * As variáveis intermediárias comuns a todos os tipos são:
 *
 * @f[
 *   \begin{aligned}
 *     A      &= 10^{\,\text{gainDB}/40}
 *               \quad(\text{apenas peaking/shelf}) \\
 *     \omega_0 &= \frac{2\pi f_0}{f_s} \\
 *     \cos\omega_0,\; \sin\omega_0 &\;\text{(do }\omega_0\text{)} \\
 *     \alpha &= \frac{\sin\omega_0}{2Q}
 *   \end{aligned}
 * @f]
 *
 * onde @f$ f_0 @f$ é a frequência central/de corte, @f$ f_s @f$ a taxa de
 * amostragem, @f$ Q @f$ o fator de qualidade e @f$ A @f$ a amplitude linear do
 * ganho (para peaking/shelf). Os coeficientes não-normalizados por tipo são
 * dados nas seções de @ref Type. Em seguida normaliza-se dividindo todos por
 * @f$ a_0 @f$, de modo que a recorrência DF2T use @f$ a_0 = 1 @f$.
 *
 * @section interp Significado de Q por tipo
 *
 * - LowPass/HighPass: @f$ Q @f$ controla o pico de ressonância na frequência de
 *   corte (@f$ Q = 1/\sqrt{2} \approx 0.707 @f$ → Butterworth, sem pico).
 * - BandPass/Notch/AllPass: @f$ Q = f_0/\text{BW} @f$ relaciona-se à largura de
 *   banda; maior @f$ Q @f$ ⇒ banda mais estreita.
 * - PeakingEQ: @f$ Q @f$ define a largura do realce/atenuação no centro.
 * - Low/HighShelf: a inclinação é controlada por @f$ Q @f$ (aqui usamos a forma
 *   com @f$ \alpha = \sin\omega_0/(2Q) @f$; @f$ Q = 1/\sqrt{2} @f$ dá uma shelf
 *   sem overshoot — equivalente a `S = 1` no cookbook).
 *
 * @section stability Estabilidade numérica
 *
 * - @f$ f_0 @f$ é fixado em @f$ [\,\epsilon,\; 0.5 f_s - \epsilon\,] @f$ para
 *   manter o pólo dentro do círculo unitário e evitar @f$ \sin/\cos @f$ em
 *   Nyquist exato.
 * - @f$ Q @f$ é fixado a um mínimo positivo para evitar divisão por zero.
 * - `updateCoefficients()` calcula @f$ \sin/\cos/\sqrt{} @f$ uma única vez e
 *   normaliza por @f$ a_0 @f$; `process()` usa apenas multiplicações/somas.
 * - Proteção contra denormais é aplicada aos estados realimentados.
 *
 * @section rt Garantias de tempo real
 *
 * - `process()` é O(1), `noexcept`, sem alocação/exceções/RTTI.
 * - `updateCoefficients()` deve ser chamado fora do audio thread quando
 *   possível (usa funções transcendentais); ainda assim não aloca nem lança.
 */
namespace cvdsp::filters
{

/**
 * @brief Tipos de filtro biquad suportados (RBJ Audio EQ Cookbook).
 *
 * Coeficientes não-normalizados (antes de dividir por @f$ a_0 @f$), com
 * @f$ c = \cos\omega_0 @f$, @f$ s = \sin\omega_0 @f$,
 * @f$ \alpha = s/(2Q) @f$ e @f$ A = 10^{\text{gainDB}/40} @f$:
 *
 * - @b LowPass:
 *   @f$ b_0=\frac{1-c}{2},\; b_1=1-c,\; b_2=\frac{1-c}{2} @f$;
 *   @f$ a_0=1+\alpha,\; a_1=-2c,\; a_2=1-\alpha @f$.
 * - @b HighPass:
 *   @f$ b_0=\frac{1+c}{2},\; b_1=-(1+c),\; b_2=\frac{1+c}{2} @f$;
 *   @f$ a_0=1+\alpha,\; a_1=-2c,\; a_2=1-\alpha @f$.
 * - @b BandPass (ganho de pico 0 dB):
 *   @f$ b_0=\alpha,\; b_1=0,\; b_2=-\alpha @f$;
 *   @f$ a_0=1+\alpha,\; a_1=-2c,\; a_2=1-\alpha @f$.
 * - @b Notch:
 *   @f$ b_0=1,\; b_1=-2c,\; b_2=1 @f$;
 *   @f$ a_0=1+\alpha,\; a_1=-2c,\; a_2=1-\alpha @f$.
 * - @b AllPass:
 *   @f$ b_0=1-\alpha,\; b_1=-2c,\; b_2=1+\alpha @f$;
 *   @f$ a_0=1+\alpha,\; a_1=-2c,\; a_2=1-\alpha @f$.
 * - @b PeakingEQ:
 *   @f$ b_0=1+\alpha A,\; b_1=-2c,\; b_2=1-\alpha A @f$;
 *   @f$ a_0=1+\alpha/A,\; a_1=-2c,\; a_2=1-\alpha/A @f$.
 * - @b LowShelf (com @f$ \beta = 2\sqrt{A}\,\alpha @f$):
 *   @f$ b_0=A[(A+1)-(A-1)c+\beta],\;
 *       b_1=2A[(A-1)-(A+1)c],\;
 *       b_2=A[(A+1)-(A-1)c-\beta] @f$;
 *   @f$ a_0=(A+1)+(A-1)c+\beta,\;
 *       a_1=-2[(A-1)+(A+1)c],\;
 *       a_2=(A+1)+(A-1)c-\beta @f$.
 * - @b HighShelf (com @f$ \beta = 2\sqrt{A}\,\alpha @f$):
 *   @f$ b_0=A[(A+1)+(A-1)c+\beta],\;
 *       b_1=-2A[(A-1)+(A+1)c],\;
 *       b_2=A[(A+1)+(A-1)c-\beta] @f$;
 *   @f$ a_0=(A+1)-(A-1)c+\beta,\;
 *       a_1=2[(A-1)-(A+1)c],\;
 *       a_2=(A+1)-(A-1)c-\beta @f$.
 */
enum class BiquadType
{
    LowPass,  ///< Passa-baixas de 2ª ordem.
    HighPass, ///< Passa-altas de 2ª ordem.
    BandPass, ///< Passa-banda (ganho de pico 0 dB).
    Notch,    ///< Rejeita-banda (band-reject).
    AllPass,  ///< Passa-tudo (resposta de fase, magnitude plana).
    PeakingEQ,///< Realce/atenuação em sino (bell), usa gainDB.
    LowShelf, ///< Prateleira grave, usa gainDB.
    HighShelf ///< Prateleira aguda, usa gainDB.
};

/**
 * @class Biquad
 * @brief Filtro biquad RBJ com processamento em Direct Form II Transposed.
 *
 * @tparam T Tipo de ponto flutuante do processamento (`float` ou `double`).
 *
 * @par Exemplo — passa-baixas ressonante:
 * @code
 *   cvdsp::filters::Biquad<float> lp;
 *   lp.prepare(48000.0f);
 *   lp.setType(cvdsp::filters::BiquadType::LowPass);
 *   lp.setFrequency(1000.0f);
 *   lp.setQ(2.0f);
 *   lp.updateCoefficients();           // recalcula os 5 coeficientes
 *   for (std::size_t n = 0; n < numSamples; ++n)
 *       buffer[n] = lp.process(buffer[n]);
 * @endcode
 *
 * @par Exemplo — peaking EQ (+6 dB em 3 kHz):
 * @code
 *   cvdsp::filters::Biquad<double> peak;
 *   peak.prepare(96000.0);
 *   peak.setType(cvdsp::filters::BiquadType::PeakingEQ);
 *   peak.setFrequency(3000.0);
 *   peak.setQ(1.0);
 *   peak.setGainDB(6.0);
 *   peak.updateCoefficients();
 *   double y = peak.process(x);
 * @endcode
 *
 * @par Exemplo — alterar parâmetros em tempo real:
 * @code
 *   // No control thread / bloco: atualize parâmetros e recoeficientes.
 *   filter.setFrequency(newFreq);
 *   filter.setQ(newQ);
 *   filter.updateCoefficients();       // seguro fora do hot loop
 *   // No audio thread: apenas process().
 * @endcode
 */
template <typename T>
class Biquad
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "Biquad requires a floating point type (float or double)");

    /// @brief Tipo escalar de ponto flutuante usado pelo filtro.
    using value_type = T;
    /// @brief Tipo inteiro sem sinal para contagens e índices.
    using size_type = std::size_t;

    /**
     * @brief Constrói um biquad neutro (passthrough) estável.
     *
     * Estado zerado e coeficientes de identidade (@f$ b_0=1 @f$, demais 0),
     * até que @ref prepare / @ref updateCoefficients sejam chamados. Não aloca.
     */
    constexpr Biquad() noexcept = default;

    /// @brief Destrutor trivial; nada é alocado.
    ~Biquad() noexcept = default;

    Biquad(const Biquad&) noexcept = default;
    Biquad& operator=(const Biquad&) noexcept = default;
    Biquad(Biquad&&) noexcept = default;
    Biquad& operator=(Biquad&&) noexcept = default;

    /**
     * @brief Prepara o filtro para uma taxa de amostragem e recalcula coefs.
     *
     * Deve ser chamado fora do audio thread. Armazena @f$ f_s @f$, zera o
     * estado interno e chama @ref updateCoefficients com os parâmetros atuais.
     *
     * @param sampleRate Taxa de amostragem em Hz (> 0; inválidos viram 48 kHz).
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);
        reset();
        updateCoefficients();
    }

    /**
     * @brief Zera o estado interno (@f$ s_1, s_2 @f$) do filtro.
     *
     * Preserva os coeficientes. Real-time safe: O(1), sem alocação/exceções.
     */
    inline void reset() noexcept
    {
        m_s1 = static_cast<value_type>(0);
        m_s2 = static_cast<value_type>(0);
    }

    /**
     * @brief Define o tipo de filtro (ver @ref BiquadType).
     *
     * Apenas armazena o parâmetro; chame @ref updateCoefficients para aplicar.
     * @param type Novo tipo de filtro.
     */
    inline void setType(BiquadType type) noexcept { m_type = type; }

    /**
     * @brief Define a frequência central/de corte @f$ f_0 @f$ em Hz.
     *
     * O valor é fixado a @f$ [\epsilon,\, 0.5 f_s - \epsilon] @f$ em
     * @ref updateCoefficients. Apenas armazena; chame @ref updateCoefficients.
     * @param frequencyHz Frequência em Hz.
     */
    inline void setFrequency(value_type frequencyHz) noexcept
    {
        m_frequency = frequencyHz;
    }

    /**
     * @brief Define o fator de qualidade @f$ Q @f$.
     *
     * Fixado a um mínimo positivo em @ref updateCoefficients para evitar divisão
     * por zero. Apenas armazena; chame @ref updateCoefficients.
     * @param q Fator de qualidade (> 0).
     */
    inline void setQ(value_type q) noexcept { m_q = q; }

    /**
     * @brief Define o ganho em decibéis (apenas Peaking/LowShelf/HighShelf).
     *
     * Ignorado pelos demais tipos. Apenas armazena; chame
     * @ref updateCoefficients.
     * @param gainDB Ganho em dB (positivo = realce, negativo = atenuação).
     */
    inline void setGainDB(value_type gainDB) noexcept { m_gainDB = gainDB; }


    /**
     * @brief Configura e aplica um filtro peaking EQ em uma chamada.
     */
    inline void setPeak(
        value_type sampleRate,
        value_type frequencyHz,
        value_type q,
        value_type gainDB) noexcept
    {
        prepare(sampleRate);
        setType(BiquadType::PeakingEQ);
        setFrequency(frequencyHz);
        setQ(q);
        setGainDB(gainDB);
        updateCoefficients();
    }

    /**
     * @brief Configura e aplica uma low-shelf em uma chamada.
     */
    inline void setLowShelf(
        value_type sampleRate,
        value_type frequencyHz,
        value_type q,
        value_type gainDB) noexcept
    {
        prepare(sampleRate);
        setType(BiquadType::LowShelf);
        setFrequency(frequencyHz);
        setQ(q);
        setGainDB(gainDB);
        updateCoefficients();
    }

    /**
     * @brief Configura e aplica uma high-shelf em uma chamada.
     */
    inline void setHighShelf(
        value_type sampleRate,
        value_type frequencyHz,
        value_type q,
        value_type gainDB) noexcept
    {
        prepare(sampleRate);
        setType(BiquadType::HighShelf);
        setFrequency(frequencyHz);
        setQ(q);
        setGainDB(gainDB);
        updateCoefficients();
    }

    /**
     * @brief Recalcula os coeficientes a partir dos parâmetros atuais.
     *
     * Implementa as fórmulas do RBJ Audio EQ Cookbook para o @ref BiquadType
     * selecionado, normalizando por @f$ a_0 @f$. Usa @f$ \sin/\cos/\sqrt{} @f$;
     * portanto prefira chamar fora do hot loop de áudio. Não aloca, não lança.
     */
    inline void updateCoefficients() noexcept
    {
        const value_type one = static_cast<value_type>(1);
        const value_type two = static_cast<value_type>(2);
        const value_type half = static_cast<value_type>(0.5);

        // Clamp de estabilidade para f0 e Q.
        const value_type nyquist = m_sampleRate * half;
        const value_type eps = static_cast<value_type>(1e-6);
        const value_type f0 =
            std::clamp(m_frequency, eps, nyquist - eps);
        const value_type q =
            std::max(m_q, static_cast<value_type>(1e-4));

        // Variáveis intermediárias do cookbook.
        const value_type w0 = cvdsp::twoPi<value_type> * f0 / m_sampleRate;
        const value_type cosw0 = std::cos(w0);
        const value_type sinw0 = std::sin(w0);
        const value_type alpha = sinw0 / (two * q);
        // A = 10^(gainDB/40) = sqrt(10^(gainDB/20)).
        const value_type A =
            std::pow(static_cast<value_type>(10),
                     m_gainDB / static_cast<value_type>(40));

        // Coeficientes não-normalizados.
        value_type b0, b1, b2, a0, a1, a2;

        switch (m_type)
        {
            case BiquadType::LowPass:
            {
                b0 = (one - cosw0) * half;
                b1 = one - cosw0;
                b2 = (one - cosw0) * half;
                a0 = one + alpha;
                a1 = -two * cosw0;
                a2 = one - alpha;
                break;
            }
            case BiquadType::HighPass:
            {
                b0 = (one + cosw0) * half;
                b1 = -(one + cosw0);
                b2 = (one + cosw0) * half;
                a0 = one + alpha;
                a1 = -two * cosw0;
                a2 = one - alpha;
                break;
            }
            case BiquadType::BandPass: // pico de 0 dB (constant peak gain)
            {
                b0 = alpha;
                b1 = static_cast<value_type>(0);
                b2 = -alpha;
                a0 = one + alpha;
                a1 = -two * cosw0;
                a2 = one - alpha;
                break;
            }
            case BiquadType::Notch:
            {
                b0 = one;
                b1 = -two * cosw0;
                b2 = one;
                a0 = one + alpha;
                a1 = -two * cosw0;
                a2 = one - alpha;
                break;
            }
            case BiquadType::AllPass:
            {
                b0 = one - alpha;
                b1 = -two * cosw0;
                b2 = one + alpha;
                a0 = one + alpha;
                a1 = -two * cosw0;
                a2 = one - alpha;
                break;
            }
            case BiquadType::PeakingEQ:
            {
                b0 = one + alpha * A;
                b1 = -two * cosw0;
                b2 = one - alpha * A;
                a0 = one + alpha / A;
                a1 = -two * cosw0;
                a2 = one - alpha / A;
                break;
            }
            case BiquadType::LowShelf:
            {
                const value_type sqrtA = std::sqrt(A);
                const value_type beta = two * sqrtA * alpha;
                b0 = A * ((A + one) - (A - one) * cosw0 + beta);
                b1 = two * A * ((A - one) - (A + one) * cosw0);
                b2 = A * ((A + one) - (A - one) * cosw0 - beta);
                a0 = (A + one) + (A - one) * cosw0 + beta;
                a1 = -two * ((A - one) + (A + one) * cosw0);
                a2 = (A + one) + (A - one) * cosw0 - beta;
                break;
            }
            case BiquadType::HighShelf:
            {
                const value_type sqrtA = std::sqrt(A);
                const value_type beta = two * sqrtA * alpha;
                b0 = A * ((A + one) + (A - one) * cosw0 + beta);
                b1 = -two * A * ((A - one) + (A + one) * cosw0);
                b2 = A * ((A + one) + (A - one) * cosw0 - beta);
                a0 = (A + one) - (A - one) * cosw0 + beta;
                a1 = two * ((A - one) - (A + one) * cosw0);
                a2 = (A + one) - (A - one) * cosw0 - beta;
                break;
            }
            default:
            {
                // Passthrough seguro (identidade) — nunca alcançado.
                b0 = one; b1 = static_cast<value_type>(0);
                b2 = static_cast<value_type>(0);
                a0 = one; a1 = static_cast<value_type>(0);
                a2 = static_cast<value_type>(0);
                break;
            }
        }

        // Normalização por a0 (DF2T usa a0 = 1).
        const value_type invA0 = one / a0;
        m_b0 = b0 * invA0;
        m_b1 = b1 * invA0;
        m_b2 = b2 * invA0;
        m_a1 = a1 * invA0;
        m_a2 = a2 * invA0;
    }

    /**
     * @brief Processa uma única amostra (Direct Form II Transposed).
     *
     * @param x Amostra de entrada @f$ x[n] @f$.
     * @return Amostra de saída @f$ y[n] @f$.
     *
     * Real-time safe: O(1), `noexcept`, sem alocação/exceções/RTTI. Aplica
     * proteção contra denormais aos estados realimentados.
     */
    [[nodiscard]] inline value_type process(value_type x) noexcept
    {
        // y = b0*x + s1
        const value_type y = m_b0 * x + m_s1;
        // s1 = b1*x - a1*y + s2
        m_s1 = m_b1 * x - m_a1 * y + m_s2 + kDenormalGuard - kDenormalGuard;
        // s2 = b2*x - a2*y
        m_s2 = m_b2 * x - m_a2 * y + kDenormalGuard - kDenormalGuard;
        return y;
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

    /// @brief Tipo de filtro atual.
    [[nodiscard]] constexpr BiquadType getType() const noexcept { return m_type; }
    /// @brief Frequência central/de corte configurada (Hz).
    [[nodiscard]] constexpr value_type getFrequency() const noexcept { return m_frequency; }
    /// @brief Fator de qualidade configurado.
    [[nodiscard]] constexpr value_type getQ() const noexcept { return m_q; }
    /// @brief Ganho configurado em dB.
    [[nodiscard]] constexpr value_type getGainDB() const noexcept { return m_gainDB; }
    /// @brief Taxa de amostragem configurada (Hz).
    [[nodiscard]] constexpr value_type getSampleRate() const noexcept { return m_sampleRate; }

private:
    /// @brief Constante anti-denormal para os estados realimentados.
    static constexpr value_type kDenormalGuard =
        static_cast<value_type>(1e-20);

    // Coeficientes normalizados (a0 = 1).
    value_type m_b0{static_cast<value_type>(1)}; ///< Feedforward b0.
    value_type m_b1{static_cast<value_type>(0)}; ///< Feedforward b1.
    value_type m_b2{static_cast<value_type>(0)}; ///< Feedforward b2.
    value_type m_a1{static_cast<value_type>(0)}; ///< Feedback a1.
    value_type m_a2{static_cast<value_type>(0)}; ///< Feedback a2.

    // Estados DF2T.
    value_type m_s1{static_cast<value_type>(0)}; ///< Estado s1.
    value_type m_s2{static_cast<value_type>(0)}; ///< Estado s2.

    // Parâmetros.
    BiquadType m_type{BiquadType::LowPass};                 ///< Tipo de filtro.
    value_type m_frequency{static_cast<value_type>(1000)};  ///< f0 (Hz).
    value_type m_q{static_cast<value_type>(0.70710678118654752440)}; ///< Q.
    value_type m_gainDB{static_cast<value_type>(0)};        ///< Ganho (dB).
    value_type m_sampleRate{static_cast<value_type>(48000)};///< fs (Hz).
};

} // namespace cvdsp::filters

#endif // CVDSP_FILTERS_BIQUAD_HPP
