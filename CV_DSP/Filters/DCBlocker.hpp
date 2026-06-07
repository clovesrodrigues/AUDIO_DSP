#ifndef CVDSP_FILTERS_DCBLOCKER_HPP
#define CVDSP_FILTERS_DCBLOCKER_HPP

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <algorithm>

#include "../Core/Types.hpp"
#include "../Core/Constants.hpp"

/**
 * @file DCBlocker.hpp
 * @brief First-order DC-blocking (DC offset removal) filter for CV_DSP.
 * @namespace cvdsp::filters
 *
 * @section overview Visão geral
 *
 * Este módulo implementa um filtro removedor de componente DC (corrente
 * contínua), também conhecido como "DC blocker" ou "DC trap". É um filtro
 * passa-altas de primeira ordem extremamente leve, projetado para rodar dentro
 * do callback de áudio em tempo real, sem alocação dinâmica, sem exceções e
 * sem RTTI.
 *
 * @section equation Equação de diferenças
 *
 * A equação implementada é:
 *
 * @f[
 *     y[n] = x[n] - x[n-1] + R \cdot y[n-1]
 * @f]
 *
 * onde:
 * - @f$ x[n] @f$  é a amostra de entrada atual;
 * - @f$ x[n-1] @f$ é a amostra de entrada anterior;
 * - @f$ y[n-1] @f$ é a amostra de saída anterior;
 * - @f$ R @f$ é o coeficiente do pólo (real, @f$ 0 \le R < 1 @f$),
 *   tipicamente próximo de 1 (ex.: 0.995 a 0.9995).
 *
 * @section transfer Função de transferência
 *
 * Aplicando a transformada Z à equação de diferenças:
 *
 * @f[
 *     Y(z) = X(z) - z^{-1} X(z) + R z^{-1} Y(z)
 * @f]
 *
 * @f[
 *     H(z) = \frac{Y(z)}{X(z)} = \frac{1 - z^{-1}}{1 - R z^{-1}}
 * @f]
 *
 * Observamos:
 * - Um **zero** em @f$ z = 1 @f$ (DC, @f$ \omega = 0 @f$). Como o ganho do
 *   zero anula exatamente a frequência zero, qualquer offset constante (DC)
 *   é levado a 0 no regime permanente. Esse é o mecanismo central do filtro.
 * - Um **pólo** em @f$ z = R @f$ sobre o eixo real. Quanto mais próximo de 1,
 *   mais estreita é a banda rejeitada em torno de DC (corte mais baixo) e mais
 *   "transparente" o filtro fica para o áudio audível.
 *
 * Em @f$ z = 1 @f$ (DC): @f$ H(1) = \frac{1-1}{1-R} = 0 @f$ → DC totalmente
 * removido. Em altas frequências (@f$ z = -1 @f$, Nyquist):
 * @f$ H(-1) = \frac{2}{1+R} \approx 1 @f$ → sinal de áudio praticamente
 * inalterado.
 *
 * @section cutoff Frequência de corte (-3 dB)
 *
 * A frequência de corte aproximada onde a resposta cai 3 dB relaciona-se com
 * @f$ R @f$ por:
 *
 * @f[
 *     f_c \approx \frac{f_s}{2\pi}\,(1 - R)
 *     \quad\Longleftrightarrow\quad
 *     R \approx 1 - \frac{2\pi f_c}{f_s}
 * @f]
 *
 * para @f$ f_c \ll f_s @f$. Esta aproximação é usada por
 * @ref DCBlocker::setCutoffHz para oferecer uma interface em Hz, enquanto
 * @ref DCBlocker::setCoefficient permite controlar @f$ R @f$ diretamente.
 *
 * @section stability Estabilidade numérica
 *
 * O filtro é estável enquanto @f$ |R| < 1 @f$ (pólo dentro do círculo
 * unitário). A implementação faz clamp de @f$ R @f$ em
 * @f$ [0, R_{max}] @f$ com @f$ R_{max} < 1 @f$ para garantir estabilidade
 * mesmo diante de entradas inválidas, e aplica proteção contra denormais no
 * estado de realimentação para evitar picos de CPU no callback de áudio.
 *
 * @section why Por que remover DC? — impacto na cadeia DSP
 *
 * Um offset DC é uma componente constante somada ao sinal de áudio. Ele é
 * inaudível por si só (0 Hz está abaixo da audição), mas causa estragos sérios
 * quando o sinal passa por estágios não-lineares ou dependentes de energia,
 * muito comuns em modelagem de amplificadores de guitarra:
 *
 * - @b DC @b Offset: desloca a forma de onda para cima ou para baixo em torno
 *   de zero. Reduz o headroom disponível (a onda "encosta" mais cedo em um dos
 *   trilhos), pode produzir cliques em edições/automação e desperdiça faixa
 *   dinâmica.
 *
 * - @b Saturação (waveshaping / clipping): funções não-lineares como
 *   @f$ \tanh(x) @f$, clipping suave/duro etc. são assimétricas em torno de um
 *   sinal deslocado. Com DC presente, a saturação gera componentes harmônicas
 *   pares espúrias e um novo offset, "bombeando" a forma de onda. Colocar um
 *   DC blocker @b antes e/ou @b depois de cada estágio de distorção mantém o
 *   sinal centrado em zero e a saturação simétrica e previsível.
 *
 * - @b Convolution (IRs de gabinete/reverb): a convolução com uma resposta ao
 *   impulso preserva (e pode amplificar) o DC presente na entrada. Pior: muitas
 *   IRs de cabinet têm ganho não-nulo em DC, podendo acumular offset ao longo
 *   do tempo. Remover DC antes da convolução evita acúmulo de offset e mantém
 *   a IR operando na sua faixa linear.
 *
 * - @b Compressão (dinâmica): detectores de envelope (peak/RMS) medem energia.
 *   Um offset DC infla a leitura do detector, fazendo o compressor "agarrar"
 *   incorretamente, alterar o ganho de redução e bombear. Um DC blocker no
 *   início da cadeia garante medição de envelope honesta.
 *
 * @b Posição @b típica @b na @b cadeia: o DC blocker é normalmente o primeiro
 * elo (limpeza da entrada) e também é inserido entre estágios não-lineares
 * (distorção → DC blocker → próximo estágio) num pedalboard/amp sim:
 *
 * @code
 *   entrada → [DC Blocker] → ganho/drive → [waveshaper] → [DC Blocker]
 *           → tone stack → [convolution IR] → compressor → saída
 * @endcode
 *
 * @section rt Garantias de tempo real
 *
 * - Header-only, C++20, sem dependências externas.
 * - @ref DCBlocker::process é O(1): apenas multiplicações e somas por amostra.
 * - Sem `new`/`delete`/`malloc`/`free`, sem exceções, sem RTTI.
 * - `noexcept` e `constexpr` aplicados onde possível.
 */
namespace cvdsp::filters
{

/**
 * @class DCBlocker
 * @brief Filtro removedor de DC de primeira ordem (passa-altas leve).
 *
 * Implementa @f$ y[n] = x[n] - x[n-1] + R\,y[n-1] @f$.
 *
 * @tparam T Tipo de ponto flutuante do processamento (`float` ou `double`).
 *
 * Fluxo de uso:
 * @code
 *   cvdsp::filters::DCBlocker<float> dc;
 *   dc.prepare(48000.0f);      // fora do audio thread
 *   dc.setCutoffHz(20.0f);     // ou dc.setCoefficient(0.9995f);
 *   // ... no callback de áudio:
 *   for (std::size_t n = 0; n < numSamples; ++n)
 *       buffer[n] = dc.process(buffer[n]);
 * @endcode
 */
template <typename T>
class DCBlocker
{
public:
    static_assert(std::is_floating_point_v<T>,
                  "DCBlocker requires a floating point type (float or double)");

    /// @brief Tipo escalar de ponto flutuante usado pelo filtro.
    using value_type = T;
    /// @brief Tipo inteiro sem sinal para contagens e índices.
    using size_type = std::size_t;

    /**
     * @brief Constrói um DC blocker com coeficiente padrão estável.
     *
     * O estado interno é zerado e @f$ R @f$ recebe um valor padrão conservador
     * (@ref kDefaultCoefficient), adequado a um corte muito baixo. Nenhuma
     * alocação é realizada.
     */
    constexpr DCBlocker() noexcept = default;

    /// @brief Destrutor trivial; não libera recursos (nada é alocado).
    ~DCBlocker() noexcept = default;

    DCBlocker(const DCBlocker&) noexcept = default;
    DCBlocker& operator=(const DCBlocker&) noexcept = default;
    DCBlocker(DCBlocker&&) noexcept = default;
    DCBlocker& operator=(DCBlocker&&) noexcept = default;

    /**
     * @brief Prepara o filtro para uma dada taxa de amostragem.
     *
     * Deve ser chamado fora do audio thread (ex.: em @c prepareToPlay /
     * @c setupProcessing). Armazena a taxa de amostragem (usada por
     * @ref setCutoffHz para converter Hz em coeficiente) e zera o estado
     * interno para evitar transientes residuais.
     *
     * @param sampleRate Taxa de amostragem em Hz. Deve ser > 0; valores
     *        inválidos são substituídos por um padrão seguro.
     *
     * @note Real-time safe não é obrigatório aqui (uso fora do callback), mas
     *       a função ainda assim não aloca, não lança e é O(1).
     */
    inline void prepare(value_type sampleRate) noexcept
    {
        m_sampleRate = (sampleRate > static_cast<value_type>(0))
                           ? sampleRate
                           : static_cast<value_type>(48000);
        reset();
    }

    /**
     * @brief Zera completamente o estado interno do filtro.
     *
     * Limpa @f$ x[n-1] @f$ e @f$ y[n-1] @f$. Os coeficientes (R) são
     * preservados. Real-time safe: O(1), sem alocação, sem exceções.
     */
    inline void reset() noexcept
    {
        m_x1 = static_cast<value_type>(0);
        m_y1 = static_cast<value_type>(0);
    }

    /**
     * @brief Define diretamente o coeficiente do pólo @f$ R @f$.
     *
     * @param coefficient Valor desejado de @f$ R @f$. Para estabilidade, o
     *        valor é fixado (clamp) ao intervalo @f$ [0, R_{max}] @f$, com
     *        @f$ R_{max} = @f$ @ref kMaxCoefficient @f$ < 1 @f$.
     *
     * Interpretação: quanto mais próximo de 1, mais baixa a frequência de corte
     * e mais transparente o filtro fica para o áudio; valores menores afastam o
     * corte para cima, removendo também graves muito baixos.
     *
     * Real-time safe: O(1), sem alocação, sem exceções.
     */
    inline void setCoefficient(value_type coefficient) noexcept
    {
        m_R = std::clamp(coefficient,
                         static_cast<value_type>(0),
                         kMaxCoefficient);
    }

    /**
     * @brief Define o coeficiente a partir de uma frequência de corte em Hz.
     *
     * Usa a aproximação de primeira ordem
     * @f$ R \approx 1 - \dfrac{2\pi f_c}{f_s} @f$, válida para
     * @f$ f_c \ll f_s @f$. O resultado é fixado ao intervalo estável via
     * @ref setCoefficient.
     *
     * @param cutoffHz Frequência de corte (-3 dB) desejada, em Hz. Valores
     *        não-positivos resultam em @f$ R = R_{max} @f$ (corte mínimo).
     *
     * @pre @ref prepare deve ter sido chamado para que @c m_sampleRate seja
     *      válido. Caso contrário, usa-se o padrão de 48 kHz.
     *
     * Real-time safe: O(1), sem alocação, sem exceções.
     */
    inline void setCutoffHz(value_type cutoffHz) noexcept
    {
        if (cutoffHz <= static_cast<value_type>(0))
        {
            setCoefficient(kMaxCoefficient);
            return;
        }
        const value_type r =
            static_cast<value_type>(1) -
            (cvdsp::twoPi<value_type> * cutoffHz) / m_sampleRate;
        setCoefficient(r);
    }

    /**
     * @brief Retorna o coeficiente do pólo @f$ R @f$ atualmente em uso.
     * @return Valor de @f$ R @f$ no intervalo @f$ [0, R_{max}] @f$.
     */
    [[nodiscard]] constexpr value_type getCoefficient() const noexcept
    {
        return m_R;
    }

    /**
     * @brief Retorna a taxa de amostragem configurada (Hz).
     * @return Taxa de amostragem atual.
     */
    [[nodiscard]] constexpr value_type getSampleRate() const noexcept
    {
        return m_sampleRate;
    }

    /**
     * @brief Processa uma única amostra, removendo a componente DC.
     *
     * Implementa @f$ y[n] = x[n] - x[n-1] + R\,y[n-1] @f$ e atualiza o estado
     * interno (@f$ x[n-1] \leftarrow x[n] @f$, @f$ y[n-1] \leftarrow y[n] @f$).
     *
     * @param x Amostra de entrada @f$ x[n] @f$.
     * @return Amostra de saída @f$ y[n] @f$ com DC removido.
     *
     * Real-time safe: O(1), sem alocação, sem exceções, sem RTTI. Aplica
     * proteção contra denormais ao estado realimentado para evitar picos de CPU.
     */
    [[nodiscard]] inline value_type process(value_type x) noexcept
    {
        // y[n] = x[n] - x[n-1] + R * y[n-1]
        value_type y = x - m_x1 + m_R * m_y1;

        // Proteção contra denormais: um sinal de áudio que decai a zero pode
        // levar y[n-1] a valores subnormais, causando lentidão extrema em
        // algumas CPUs dentro do callback. Somar e subtrair uma constante
        // muito pequena ("DC anti-denormal") empurra o estado para zero exato.
        y += kDenormalGuard;
        y -= kDenormalGuard;

        // Atualiza o histórico (estado).
        m_x1 = x;
        m_y1 = y;
        return y;
    }

    /**
     * @brief Processa um bloco de amostras in-place.
     *
     * Conveniência para hosts que entregam buffers contíguos. Equivale a
     * chamar @ref process para cada amostra, na ordem.
     *
     * @param buffer Ponteiro para as amostras (não-nulo se @p numSamples > 0).
     * @param numSamples Quantidade de amostras a processar.
     *
     * Real-time safe: O(N), sem alocação, sem exceções.
     */
    inline void processBlock(value_type* buffer, size_type numSamples) noexcept
    {
        for (size_type n = 0; n < numSamples; ++n)
            buffer[n] = process(buffer[n]);
    }

    /**
     * @brief Coeficiente padrão de @f$ R @f$ (corte muito baixo, ~ alguns Hz).
     */
    static constexpr value_type kDefaultCoefficient =
        static_cast<value_type>(0.9995);

    /**
     * @brief Limite superior de @f$ R @f$ para garantir estabilidade
     *        (pólo estritamente dentro do círculo unitário).
     */
    static constexpr value_type kMaxCoefficient =
        static_cast<value_type>(0.99999);

private:
    /**
     * @brief Constante anti-denormal somada/subtraída ao estado realimentado.
     *
     * Pequena o bastante para não alterar audivelmente o sinal, grande o
     * bastante para escapar da faixa subnormal de @c T.
     */
    static constexpr value_type kDenormalGuard =
        static_cast<value_type>(1e-20);

    value_type m_R{kDefaultCoefficient}; ///< Coeficiente do pólo R.
    value_type m_x1{static_cast<value_type>(0)}; ///< Estado x[n-1].
    value_type m_y1{static_cast<value_type>(0)}; ///< Estado y[n-1].
    value_type m_sampleRate{static_cast<value_type>(48000)}; ///< Taxa (Hz).
};

} // namespace cvdsp::filters

#endif // CVDSP_FILTERS_DCBLOCKER_HPP
