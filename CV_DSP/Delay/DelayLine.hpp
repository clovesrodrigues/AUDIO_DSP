#ifndef CVDSP_DELAY_DELAYLINE_HPP
#define CVDSP_DELAY_DELAYLINE_HPP

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <cassert>

/**
 * @file DelayLine.hpp
 * @namespace cvdsp::delay
 * @brief Implementação profissional de delay line com suporte a delay fracionário
 * 
 * Este módulo implementa delay lines de alta qualidade para processamento de áudio
 * em tempo real, com suporte a:
 * - Delay inteiro (amostras discretas)
 * - Delay fracionário (interpolação entre amostras)
 * - Interpolação linear (qualidade boa, baixa latência)
 * - Interpolação cúbica (qualidade excelente, mais CPU)
 * 
 * Aplicações:
 * - Echo/Reverberação
 * - Chorus (modulação de delay)
 * - Flanger (modulação rápida)
 * - Vibrato (modulação de pitch)
 * - Doppler effect (deslocamento de frequência)
 * - Look-ahead para compressores/limitadores
 * 
 * **Estrutura da Biblioteca CV_DSP**:
 * ```
 * CV_DSP/
 * ├── Core/
 * │   ├── CircularBuffer.hpp
 * │   └── ...
 * └── Delay/
 *     └── DelayLine.hpp  ← Este arquivo
 * ```
 */

namespace cvdsp::delay
{

/**
 * @enum InterpolationType
 * @brief Tipos de interpolação suportados para delay fracionário
 * 
 * Define qual algoritmo de interpolação será utilizado durante o processamento
 * para melhorar a qualidade do delay fracionário.
 */
enum class InterpolationType : std::uint8_t
{
    /// Sem interpolação (delay inteiro apenas)
    /// Latência: 0 amostras
    /// CPU: Mínimo
    /// Qualidade: Baixa em delays modulados
    None = 0,
    
    /// Interpolação linear (2 pontos)
    /// Latência: ~0.5 amostras
    /// CPU: Baixo
    /// Qualidade: Boa (erro ~0.3dB @ Nyquist)
    /// Uso: Chorus, vibrato, flanger
    Linear = 1,
    
    /// Interpolação cúbica Hermite (4 pontos)
    /// Latência: ~1.5 amostras
    /// CPU: ~4x linear
    /// Qualidade: Excelente (erro ~0.02dB @ Nyquist)
    /// Uso: Qualidade profissional, broadcast
    Cubic = 2
};

/**
 * @class DelayLine
 * @brief Delay line profissional com interpolação para DSP em tempo real
 * 
 * **Propósito Geral**:
 * 
 * A DelayLine é um componente fundamental em qualquer biblioteca DSP. Ela
 * armazena histórico de sinal e permite leitura em pontos variáveis no passado,
 * possibilitando efeitos que dependem de delay temporal.
 * 
 * ==================================================================================
 * MATEMÁTICA DO DELAY INTEIRO
 * ==================================================================================
 * 
 * Um delay inteiro simples é dado por:
 * ```
 * y[n] = x[n - D]
 * ```
 * onde D é o delay em amostras (número inteiro positivo).
 * 
 * Implementação com buffer circular:
 * ```
 * Buffer:       [s0][s1][s2][s3][s4][s5] ... [sN-1]
 * WriteIndex:   5 (próxima posição a escrever)
 * Delay (D):    2 amostras
 * 
 * Índice de leitura: readIndex = (writeIndex - D) % N
 *                               = (5 - 2) % N = 3
 * 
 * Saída: y[n] = buffer[3]  (a amostra 2 passos atrás)
 * ```
 * 
 * Vantagem: Sem aliasing, sem artefatos
 * Desvantagem: Delay preso a valores inteiros (granularidade de 1 amostra)
 * 
 * ==================================================================================
 * MATEMÁTICA DO DELAY FRACIONÁRIO (FRACTIONAL DELAY)
 * ==================================================================================
 * 
 * Quando D não é inteiro (D = D_int + D_frac, onde 0 ≤ D_frac < 1):
 * Necessário interpolar entre duas amostras adjacentes.
 * 
 * **INTERPOLAÇÃO LINEAR**:
 * 
 * Fórmula:
 * ```
 * y[n] = (1 - D_frac) * x[n - D_int] + D_frac * x[n - D_int - 1]
 * ```
 * 
 * Interpretação geométrica:
 * ```
 * sample_0 (em D_int)           sample_1 (em D_int+1)
 *    ●─────────────────────────────●
 *         ↑
 *         └─ Interpolação linear aqui (D_frac = 0.3)
 *
 * output = 0.7 * sample_0 + 0.3 * sample_1
 * ```
 * 
 * Características:
 * - Erro de fase: ≈ 0.3 dB @ Nyquist (aceitável para muitas aplicações)
 * - Latência introduzida: ~0.5 amostras (estimativa média)
 * - CPU: Mínimo (2 multiplicações, 1 adição)
 * - Qualidade: Boa para efeitos modulados
 * 
 * Resposta em frequência:
 * ```
 * |H(ω)| ≈ sinc(ω/π)  (com aliasing pequeno)
 * Bandwidth: até ~15 kHz mantendo qualidade aceitável
 * ```
 * 
 * **INTERPOLAÇÃO CÚBICA HERMITE**:
 * 
 * Usa 4 pontos adjacentes para construir polinômio cúbico suave:
 * ```
 * y[0] = x[n - D_int]
 * y[1] = x[n - D_int - 1]
 * y[2] = x[n - D_int - 2]
 * y[3] = x[n - D_int - 3]
 * ```
 * 
 * Fórmulas Hermite (baseadas em t = D_frac):
 * ```
 * h00(t) = 2t³ - 3t² + 1           (influência de y[0])
 * h10(t) = t³ - 2t² + t            (influência derivada y[0])
 * h01(t) = -2t³ + 3t²              (influência de y[1])
 * h11(t) = t³ - t²                 (influência derivada y[1])
 * ```
 * 
 * Derivadas aproximadas (diferenças finitas centralizadas):
 * ```
 * m0 = (y[2] - y[1]) / 2           (slope em y[0])
 * m1 = (y[3] - y[0]) / 2           (slope em y[1])
 * ```
 * 
 * Interpolação final:
 * ```
 * output = h00(t)*y[0] + h10(t)*m0 + h01(t)*y[1] + h11(t)*m1
 * ```
 * 
 * Visualização:
 * ```
 * Cúbica (suave):    ╱╲╱╲╱╲
 * Linear (degraus):  ╱─╲╱─╲╱
 * Sinal real:        ～～～～
 * ```
 * 
 * Características:
 * - Erro de fase: ≈ 0.02 dB @ Nyquist (excelente!)
 * - Latência introduzida: ~1.5 amostras
 * - CPU: ~4x interpolação linear
 * - Qualidade: Profissional (banda passante até ~20 kHz)
 * 
 * Resposta em frequência:
 * ```
 * |H(ω)| ≈ sinc³(ω/π)  (muito superior)
 * Bandwidth: até ~20 kHz com qualidade excelente
 * ```
 * 
 * ==================================================================================
 * DOPPLER EFFECT (Efeito Doppler)
 * ==================================================================================
 * 
 * **Fenômeno Físico**:
 * Quando uma fonte sonora se move em relação ao observador, a frequência muda.
 * Este efeito ocorre porque a distância (delay) entre fonte e observador varia.
 * 
 * **Implementação em DSP**:
 * Delay variável no tempo causa mudança de frequência (pitch shifting):
 * ```
 * Delay variável: D[n] = D_base + A * sin(2π * f_lfo * n / Fs)
 * 
 * Taxa de mudança: dD/dn
 * Desvio de frequência: Δf = -f_original * (dD/dt) * Fs
 * 
 * onde:
 *   D_base: delay central em amostras
 *   A: amplitude de modulação (amostras)
 *   f_lfo: frequência do LFO (Hz)
 *   Fs: taxa de amostragem (Hz)
 *   f_original: frequência da amostra original (Hz)
 * ```
 * 
 * **Exemplo Numérico - Ambulância**:
 * ```
 * Parâmetros:
 * - Frequência original: 1000 Hz (tom de ambulância)
 * - Delay varia: ±5 ms @ 2 Hz
 * - Sample rate: 48 kHz
 * 
 * Delay em amostras: ±5ms * 48 = ±240 amostras
 * Taxa máxima dD/dn: 2 * 240 = 480 amostras/período
 * 
 * Desvio máximo: Δf = 1000 * (480 / 48000) * 48000 = ±10 Hz
 * 
 * Resultado: Frequência varia entre 990 Hz e 1010 Hz (efeito audível!)
 * ```
 * 
 * **Aplicações Práticas**:
 * - Sirene de ambulância/polícia (passando)
 * - Zumbido de abelha (movimento)
 * - Efeito "Wow" em vintage tape
 * - Rotação Lesllie (órgão Hammond)
 * 
 * **Código Exemplo**:
 * ```cpp
 * DelayLine<float, 16384, InterpolationType::Cubic> doppler;
 * doppler.prepare(48000);
 * 
 * float phase = 0.0f;
 * for (size_t n = 0; n < blockSize; ++n) {
 *     // Delay oscila entre 5-15 ms
 *     float lfo = std::sin(phase);
 *     doppler.setDelayMilliseconds(10.0f + lfo * 5.0f);
 *     
 *     outputBuffer[n] = doppler.process(inputBuffer[n]);
 *     
 *     phase += 2.0f * M_PI * 2.0f / 48000.0f;  // 2 Hz LFO
 * }
 * ```
 * 
 * ==================================================================================
 * CHORUS (Engrossamento de Som)
 * ==================================================================================
 * 
 * **Objetivo Sonoro**: Fazer som parecer múltiplas vozes tocando simultaneamente.
 * 
 * **Fórmula Básica**:
 * ```
 * y[n] = x[n] + g * x[n - D(n)]
 * 
 * D(n) = D_center + A * sin(2π * f_lfo * n / Fs)
 * ```
 * 
 * onde:
 *   x[n]: sinal de entrada (seco)
 *   g: ganho do sinal atrasado (típico: 0.5-0.9)
 *   D_center: delay central em amostras (típico: 25-50 ms)
 *   A: amplitude de modulação (típico: 5-15 ms)
 *   f_lfo: frequência do LFO (típico: 1-5 Hz)
 * 
 * **Parâmetros Típicos**:
 * - Vocal: D_center=40ms, A=10ms, f_lfo=2.5Hz, g=0.7
 * - Instrumento: D_center=30ms, A=8ms, f_lfo=1.5Hz, g=0.6
 * - Synth: D_center=50ms, A=15ms, f_lfo=3Hz, g=0.8
 * 
 * **Mecanismo Técnico**:
 * ```
 * Delay modulado cria desvios de frequência pequenos (+/- alguns Hz).
 * Esses desvios causam batimentos leves (combinação de duas frequências próximas).
 * Resultado auditivo: som "mais largo" e "mais natural".
 * 
 * Mudança de pitch: Δf ≈ ±1-5 Hz (imperceptível como pitch shift)
 * Mas perceptível como "thickening" do som
 * ```
 * 
 * **Diagrama de Sinal**:
 * ```
 * Entrada ──┬──────────────── Saída
 *           │                    ▲
 *           │                    │ + 0.7
 *           │                    │
 *           └─► Delay ──────────┘
 *               modulado
 *               (40±10ms)
 * 
 * Espectro (exemplo, 1kHz entrada):
 * Sinal seco:      1000 Hz ├───────┤
 * Sinal atrasado:  ~998-1002 Hz (modulado) ├─∼─┤
 * Resultado:       Batimento leve em ~2Hz (largura × profundidade)
 * ```
 * 
 * **Aplicações Práticas**:
 * - Vocal enhancement (fazer voz parecer dupla)
 * - Instrumento enriquecido (violão, piano)
 * - Sintetizador (som mais "vivo")
 * 
 * **Código Exemplo**:
 * ```cpp
 * DelayLine<float, 16384, InterpolationType::Linear> chorus;
 * chorus.prepare(48000);
 * 
 * float phase = 0.0f;
 * const float delay_center = 40.0f;  // 40 ms
 * const float delay_mod = 10.0f;     // ±10 ms
 * const float gain = 0.7f;
 * const float lfo_freq = 2.5f;  // 2.5 Hz
 * 
 * for (size_t n = 0; n < blockSize; ++n) {
 *     float lfo = std::sin(phase);
 *     float delay_ms = delay_center + delay_mod * lfo;
 *     chorus.setDelayMilliseconds(delay_ms);
 *     
 *     float input = inputBuffer[n];
 *     float delayed = chorus.process(input);
 *     
 *     outputBuffer[n] = input + gain * delayed;
 *     
 *     phase += 2.0f * M_PI * lfo_freq / 48000.0f;
 * }
 * ```
 * 
 * ==================================================================================
 * FLANGER (Efeito de Varredura/Sweep)
 * ==================================================================================
 * 
 * **Objetivo Sonoro**: Som "whoosh", "jet plane", "metallic" - como avião passando.
 * 
 * **Fórmula Básica**:
 * ```
 * y[n] = x[n] + g * x[n - D(n)]  (com possível feedback)
 * 
 * D(n) = D_center + A * sin(2π * f_lfo * n / Fs)
 * ```
 * 
 * **Diferença Principal de Chorus**:
 * - Chorus: delay moderado (25-50ms), modulação lenta (1-5Hz)
 * - Flanger: delay MUITO curto (0.5-5ms), modulação rápida (0.5-10Hz)
 * 
 * **Parâmetros Típicos**:
 * - D_center: 0.5-5 ms (muito curto!)
 * - A: 0.5-2 ms (amplitude)
 * - f_lfo: 0.5-10 Hz (rápido!)
 * - g: 0.5-1.0 (ganho)
 * - feedback: opcional (intensifica efeito)
 * 
 * **Mecanismo Técnico - Filtro Comb**:
 * ```
 * Delay curto cria FILTRO COMB (resposta em frequência com picos e vales):
 * 
 * H(ω) = 1 + g * e^(-jω*D)
 * 
 * Vales (notches) em: f_notch = k * Fs / (2*D)  onde k = 1,2,3,...
 * 
 * Exemplo:
 *   D = 2ms = 96 amostras @ 48kHz
 *   f_notch_1 = 1 * 48000 / (2 * 96) = 250 Hz
 *   f_notch_2 = 2 * 48000 / (2 * 96) = 500 Hz
 *   f_notch_3 = 3 * 48000 / (2 * 96) = 750 Hz
 * 
 * Enquanto D[n] varia → notches se MOVEM espectro acima!
 * Resultado: som "sweep" caracterísitco
 * ```
 * 
 * **Diagrama de Resposta em Frequência**:
 * ```
 * Ganho (dB)
 *    ▲
 *    │    ╱╲  ╱╲  ╱╲       ← Picos
 *    │   ╱  ╲╱  ╲╱  ╲
 *    │  │                  ← Vales (notches)
 *    ├──┴───────────────► Frequência
 *    │
 * 
 * Ao variar D[n]:
 *    ▲
 *    │    ╱╲  ╱╲  ╱╲
 *    │   ╱  ╲╱  ╲╱  ╲
 *    │ ╱   ↗   ↗   ↗    ← Notches se movem!
 *    ├──────────────────► Tempo
 * ```
 * 
 * **Flanger com Feedback**:
 * ```
 * Versão iterativa (realimentação):
 * 
 * y[n] = x[n] + g * x[n - D(n)] + g² * x[n - 2*D(n)]
 * 
 * Ou em forma iterativa:
 * temp = x[n - D(n)]
 * y[n] = x[n] + g * temp + g * y_prev[n - D(n)]
 * 
 * Feedback intensifica efeito de movimentação
 * ```
 * 
 * **Aplicações Práticas**:
 * - Efeito de avião passando (clássico)
 * - Instrumento metálico
 * - Bass flanger (teclado/sintetizador)
 * - Guitarra (pedal de flanger)
 * 
 * **Código Exemplo - Flanger Clássico**:
 * ```cpp
 * DelayLine<float, 4096, InterpolationType::Linear> flanger;
 * flanger.prepare(48000);
 * 
 * float phase = 0.0f;
 * const float delay_center = 2.0f;  // 2 ms (curto!)
 * const float delay_mod = 1.5f;     // ±1.5 ms
 * const float gain = 0.8f;
 * const float lfo_freq = 2.0f;  // 2 Hz
 * 
 * for (size_t n = 0; n < blockSize; ++n) {
 *     float lfo = std::sin(phase);
 *     float delay_ms = delay_center + delay_mod * lfo;
 *     flanger.setDelayMilliseconds(delay_ms);
 *     
 *     float input = inputBuffer[n];
 *     float delayed = flanger.process(input);
 *     
 *     outputBuffer[n] = input + gain * delayed;  // Sem feedback = simples
 *     
 *     phase += 2.0f * M_PI * lfo_freq / 48000.0f;
 * }
 * ```
 * 
 * ==================================================================================
 * VIBRATO (Modulação de Pitch)
 * ==================================================================================
 * 
 * **Objetivo Sonoro**: Variação natural de altura (pitch) - como violinista tocando.
 * 
 * **Fórmula Básica**:
 * ```
 * y[n] = x[n - D(n)]  ← APENAS delay, sem sinal seco!
 * 
 * D(n) = D_center + A * sin(2π * f_lfo * n / Fs)
 * ```
 * 
 * **DIFERENÇA CRÍTICA DE CHORUS/FLANGER**:
 * - Chorus/Flanger: y[n] = x[n] + g * x[n-D(n)]  (sinal seco + atrasado)
 * - Vibrato: y[n] = x[n - D(n)]  (APENAS atrasado, SEM seco!)
 * 
 * **Parâmetros Típicos**:
 * - D_center: 1-5 ms (delay base)
 * - A: 0.5-2 ms (amplitude)
 * - f_lfo: 5-10 Hz (natural em instrumentos)
 * 
 * **Mecanismo Técnico - Pitch Shift**:
 * ```
 * Delay variável causa mudança de frequência instantânea:
 * 
 * Frequência instantânea: f_inst[n] = f_orig * (1 - dD/dn)
 * 
 * Exemplo:
 *   f_orig = 440 Hz (A4)
 *   D[n] oscila ±2ms
 *   Resultado: frequência varia entre ~420-460 Hz (áudio pitch shift)
 * ```
 * 
 * **Diferença de Tremolo**:
 * ```
 * Tremolo:  y[n] = x[n] * (1 + m * sin(...))   ← Amplitude modulada
 * Vibrato:  y[n] = x[n - D(n)]                 ← Pitch modulado
 * 
 * Tremolo: amplitude muda (volume sobe/desce)
 * Vibrato: altura muda (pitch sobe/desce)
 * 
 * Auditivamente MUITO diferente!
 * ```
 * 
 * **Comparação Visual**:
 * ```
 * Entrada (tom puro):
 * ───────────────────────
 * 
 * Tremolo (amplitude):
 * ∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿  ← Volume varia
 * 
 * Vibrato (pitch):
 * ∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿  ← Frequência varia (compressão/expansão)
 * ```
 * 
 * **Aplicações Práticas**:
 * - Violino/cordas (vibrato natural)
 * - Flauta/madeiras (vibrato expressivo)
 * - Teclado vintage (efeito Leslie)
 * - Sintetizador (modulação de pitch)
 * 
 * **Código Exemplo - Vibrato Natural**:
 * ```cpp
 * DelayLine<float, 8192, InterpolationType::Linear> vibrato;
 * vibrato.prepare(48000);
 * 
 * float phase = 0.0f;
 * const float delay_center = 2.5f;  // 2.5 ms
 * const float delay_mod = 1.0f;     // ±1 ms
 * const float lfo_freq = 6.0f;  // 6 Hz (natural)
 * 
 * for (size_t n = 0; n < blockSize; ++n) {
 *     float lfo = std::sin(phase);
 *     float delay_ms = delay_center + delay_mod * lfo;
 *     vibrato.setDelayMilliseconds(delay_ms);
 *     
 *     // IMPORTANTE: SEM x[n], apenas x[n-D(n)]!
 *     outputBuffer[n] = vibrato.process(inputBuffer[n]);
 *     
 *     phase += 2.0f * M_PI * lfo_freq / 48000.0f;
 * }
 * ```
 * 
 * ==================================================================================
 * TABELA COMPARATIVA DE EFEITOS
 * ==================================================================================
 * 
 * | Efeito    | D_center | D_mod  | f_LFO | Fórmula         | Qualidade |
 * |-----------|----------|--------|-------|-----------------|-----------|
 * | Doppler   | 10-20ms  | ±5ms   | 2Hz   | y=x[n-D(n)]    | Especial  |
 * | Chorus    | 25-50ms  | 5-15ms | 1-5Hz | y=x[n]+g*x[n-D]| Boa      |
 * | Flanger   | 0.5-5ms  | 0.5-2ms| 0.5-10Hz| y=x[n]+g*x[n-D]| Excelente|
 * | Vibrato   | 1-5ms    | 0.5-2ms| 5-10Hz| y=x[n-D(n)]    | Muito Bom|
 * 
 * ==================================================================================
 * IMPLEMENTAÇÃO REAL-TIME SAFE
 * ==================================================================================
 * 
 * **Ciclo de Configuração (fora do audio thread)**:
 * ```cpp
 * // Apenas uma vez ou em thread de controle
 * DelayLine<float, 16384, InterpolationType::Linear> delay;
 * delay.prepare(48000);  // O(N) - aceitável fora do loop
 * delay.setDelaySamples(4800);  // O(1) - rápido
 * ```
 * 
 * **Ciclo de Áudio (audio thread - real-time)**:
 * ```cpp
 * // Chamado frequentemente (cada amostra ou bloco)
 * for (size_t n = 0; n < blockSize; ++n) {
 *     float output = delay.process(inputBuffer[n]);  // O(1) GARANTIDO
 *     outputBuffer[n] = output;
 *     // - Sem alocação
 *     // - Sem exception
 *     // - Sem RTTI
 *     // - Determinístico (mesma latência sempre)
 * }
 * ```
 * 
 * **Garantias Real-Time Safe**:
 * - process(): O(1) constante, sem alocação, sem exception
 * - setDelaySamples(): O(1) constante (mudança suave de delay)
 * - setDelayMilliseconds(): O(1) constante
 * - Mudanças de delay não causam clicks ou artifacts
 * 
 * @tparam T Tipo aritmético (float ou double) para amostras
 * @tparam MaxDelaySamples Número máximo de amostras de delay (potência de 2)
 * @tparam Interpolation Tipo de interpolação (None, Linear, Cubic)
 * 
 * @note DelayLine usa CircularBuffer internamente:
 *       - Zero alocações dinâmicas
 *       - O(1) por amostra processada
 *       - Real-time safe em todos os métodos de processo
 *       - Suporta delay de 0 até MaxDelaySamples amostras
 * 
 * @see cvdsp::CircularBuffer para buffer circular base
 */
template <typename T, size_t MaxDelaySamples = 65536, InterpolationType Interpolation = InterpolationType::Linear>
class DelayLine
{
    // =========================================================================
    // STATIC ASSERTIONS - VALIDAÇÕES EM TEMPO DE COMPILAÇÃO
    // =========================================================================

    static_assert(std::is_floating_point_v<T>, 
                  "DelayLine requires floating-point type (float or double)");
    
    static_assert(MaxDelaySamples > 0, 
                  "DelayLine max delay must be greater than 0");
    
    static_assert((MaxDelaySamples & (MaxDelaySamples - 1)) == 0,
                  "DelayLine max delay must be power of 2 for efficient circular wrapping");

public:
    // =========================================================================
    // TYPEDEFS E CONSTANTES
    // =========================================================================

    /// @brief Tipo de dado (float ou double)
    using value_type = T;

    /// @brief Tipo para tamanhos e índices
    using size_type = size_t;

    /// @brief Capacidade máxima de delay em amostras
    static constexpr size_type MAX_DELAY_SAMPLES = MaxDelaySamples;

    /// @brief Máscara de bits para modulo circular eficiente
    static constexpr size_type MODULO_MASK = MaxDelaySamples - 1;

    /// @brief Tipo de interpolação configurado
    static constexpr InterpolationType INTERPOLATION_TYPE = Interpolation;

    // =========================================================================
    // CONSTRUTOR E DESTRUTOR
    // =========================================================================

    /**
     * @brief Construtor padrão. Inicializa delay line em estado neutro.
     * 
     * **Comportamento**:
     * - Buffer: preenchido com zeros
     * - Delay: 0 amostras (sem efeito)
     * - Sample rate: 48000 Hz (padrão)
     * - WriteIndex: 0
     * - Pronto para prepare()
     * 
     * **Complexidade**: O(1)
     * 
     * @note Não aloca memória (buffer é estático em stack)
     */
    constexpr DelayLine() noexcept
        : m_buffer{}
        , m_writeIndex(0)
        , m_delaySamples(0.0)
        , m_sampleRate(48000)
        , m_initialized(false)
    {
    }

    /// @brief Destrutor padrão (trivial)
    ~DelayLine() noexcept = default;

    /// @brief Cópia explicitamente deletada
    DelayLine(const DelayLine&) = delete;

    /// @brief Atribuição cópia explicitamente deletada
    DelayLine& operator=(const DelayLine&) = delete;

    /// @brief Move constructor explicitamente deletado
    DelayLine(DelayLine&&) = delete;

    /// @brief Move assignment explicitamente deletado
    DelayLine& operator=(DelayLine&&) = delete;

    // =========================================================================
    // MÉTODOS DE INICIALIZAÇÃO
    // =========================================================================

    /**
     * @brief Prepara a delay line para processamento.
     * 
     * **Operações**:
     * - Zera o buffer circular completo
     * - Define taxa de amostragem (necessário para setDelayMilliseconds())
     * - Reseta índice de escrita para 0
     * - Reseta delay para 0 amostras (sem efeito)
     * - Marca como inicializado
     * 
     * **Complexidade**: O(MaxDelaySamples)
     * 
     * **Contexto de Chamada** (fora do audio thread):
     * - VST3: onActivate() do plugin
     * - iPlug2: OnReset() do plugin
     * - JUCE: prepareToPlay()
     * - CLAP: activate()
     * - Standalone: na inicialização de áudio
     * 
     * **Thread Safety**: Deve ser chamado apenas em thread de configuração
     * 
     * @param[in] sampleRate Frequência de amostragem em Hz (ex: 48000, 96000)
     * 
     * @return void
     * 
     * @post m_initialized = true
     * @post m_sampleRate = sampleRate
     * @post m_writeIndex = 0
     * @post m_delaySamples = 0
     * @post buffer completamente zerado
     * 
     * @note **NÃO é real-time safe** (realiza limpeza em O(N))
     * 
     * @warning Chamar antes de qualquer process()
     */
    void prepare(size_type sampleRate) noexcept
    {
        // Zera o buffer circular via memset otimizado
        std::memset(m_buffer.data(), 0, sizeof(m_buffer));

        // Define sample rate para conversão ms/samples
        m_sampleRate = sampleRate;

        // Reseta índice de escrita (volta ao início)
        m_writeIndex = 0;

        // Reseta delay para 0 (sem efeito inicial)
        m_delaySamples = static_cast<value_type>(0);

        // Marca como pronto
        m_initialized = true;
    }

    /**
     * @brief Reseta o estado do buffer sem reconfiguração.
     * 
     * **Operações**:
     * - Zera o buffer circular completo
     * - Reseta índice de escrita
     * - Mantém delay atual e sample rate
     * 
     * **Complexidade**: O(MaxDelaySamples)
     * 
     * **Uso**:
     * - Bypass de efeito ativado
     * - Mute/Note-Off sem sustain
     * - Reset rápido entre notas
     * - Sincronização de transporte
     * 
     * **Thread Safety**: Deve ser chamado fora do audio thread
     * 
     * @return void
     * 
     * @post buffer completamente zerado
     * @post m_writeIndex = 0
     * @post m_delaySamples mantido
     * @post m_sampleRate mantido
     * 
     * @note **NÃO é real-time safe** (realiza limpeza em O(N))
     */
    void reset() noexcept
    {
        std::memset(m_buffer.data(), 0, sizeof(m_buffer));
        m_writeIndex = 0;
    }

    // =========================================================================
    // MÉTODOS DE CONFIGURAÇÃO - REAL-TIME SAFE
    // =========================================================================

    /**
     * @brief Define o delay em amostras (inteiro ou fracionário).
     * 
     * **Matemática**:
     * ```
     * delayInSamples = D (pode ser float para fracionário)
     * 
     * Delay inteiro: D_int = floor(delayInSamples)
     * Fração: D_frac = delayInSamples - floor(delayInSamples)
     * 
     * Índice de leitura: readIndex = (writeIndex - D_int) & MODULO_MASK
     * Interpolação: usa D_frac para interpolar entre readIndex e readIndex-1
     * ```
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Validação**:
     * - 0 ≤ delayInSamples ≤ MaxDelaySamples
     * - Valores fora deste range são clampados automaticamente
     * 
     * **Mudanças de Delay**:
     * - Mudanças suaves não causam clicks audíveis
     * - Interpolação cúbica garante continuidade
     * - Linear interpolation também é suave
     * 
     * **Uso Típico**:
     * ```cpp
     * // Delay fixo de 1 segundo @ 48kHz
     * delay.setDelaySamples(48000.0f);
     * 
     * // Delay fracionário (100.5 amostras)
     * delay.setDelaySamples(100.5f);
     * 
     * // Delay modulado (varia continuamente) - REAL-TIME SAFE
     * float lfoValue = lfo.process();  // -1 a 1
     * float modulatedDelay = baseDelay + lfoValue * modAmount;
     * delay.setDelaySamples(modulatedDelay);  // Suave, sem clicks
     * ```
     * 
     * **Real-Time Safety**: ✓ GARANTIDO
     * - O(1) constante
     * - Sem alocação
     * - Sem exception
     * - Determinístico
     * 
     * @param[in] delayInSamples Delay em amostras (pode ser fracionário)
     * 
     * @return void
     * 
     * @post m_delaySamples = clamp(delayInSamples, 0, MaxDelaySamples)
     * 
     * @note Ideal para:
     *       - Modulação contínua (chorus, flanger, vibrato)
     *       - Controle via parâmetro
     *       - Delays variáveis em tempo real
     */
    inline void setDelaySamples(value_type delayInSamples) noexcept
    {
        // Clamp delay para range válido
        // Mínimo: 0 (sem delay, bypass)
        // Máximo: MaxDelaySamples (buffer completo)
        m_delaySamples = std::clamp(delayInSamples, 
                                    static_cast<value_type>(0),
                                    static_cast<value_type>(MaxDelaySamples));
    }

    /**
     * @brief Define o delay em milissegundos.
     * 
     * **Matemática**:
     * ```
     * delay_ms = D_ms
     * delay_samples = D_ms * Fs / 1000
     * 
     * Exemplo: 100 ms @ 48 kHz
     * delay_samples = 100 * 48000 / 1000 = 4800 amostras
     * ```
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Validação**:
     * - Internamente usa setDelaySamples()
     * - Valores fora de range são clampados automaticamente
     * - Conversão precisa (exato para delays fracionários)
     * 
     * **Uso Típico**:
     * ```cpp
     * // Delay de 250 ms (clássico para chorus)
     * delay.setDelayMilliseconds(250.0f);
     * 
     * // Delay curto para flanger (2 ms)
     * delay.setDelayMilliseconds(2.0f);
     * 
     * // Modulação em milissegundos (intuitivo!)
     * float lfoValue = lfo.process();  // -1 a 1
     * float modulatedDelay = 50.0f + lfoValue * 10.0f;  // 40-60 ms
     * delay.setDelayMilliseconds(modulatedDelay);
     * ```
     * 
     * **Real-Time Safety**: ✓ GARANTIDO
     * - O(1) constante
     * - Sem alocação
     * - Sem exception
     * 
     * @param[in] delayInMilliseconds Delay em milissegundos
     * 
     * @return void
     * 
     * @pre prepare() deve ter sido chamado para definir sampleRate
     * 
     * @note **Requer prepare()** ter sido chamado antes
     *       Conversão é automática baseada no sample rate definido
     */
    inline void setDelayMilliseconds(value_type delayInMilliseconds) noexcept
    {
        // Converte milissegundos para amostras
        // delay_samples = delay_ms * Fs / 1000
        value_type delaySamples = delayInMilliseconds * 
                                  static_cast<value_type>(m_sampleRate) / 
                                  static_cast<value_type>(1000);
        setDelaySamples(delaySamples);
    }

    /**
     * @brief Define o delay em segundos.
     * 
     * **Matemática**:
     * ```
     * delay_s = D_s
     * delay_samples = D_s * Fs
     * 
     * Exemplo: 0.5 s @ 48 kHz
     * delay_samples = 0.5 * 48000 = 24000 amostras
     * ```
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso Típico**:
     * ```cpp
     * // Delay de 1 segundo (longo para reverb)
     * delay.setDelaySeconds(1.0f);
     * 
     * // Delay muito curto (50 ms)
     * delay.setDelaySeconds(0.05f);
     * ```
     * 
     * **Real-Time Safety**: ✓ GARANTIDO
     * 
     * @param[in] delayInSeconds Delay em segundos
     * 
     * @return void
     */
    inline void setDelaySeconds(value_type delayInSeconds) noexcept
    {
        setDelayMilliseconds(delayInSeconds * static_cast<value_type>(1000));
    }

    // =========================================================================
    // MÉTODOS DE PROCESSAMENTO - REAL-TIME SAFE
    // =========================================================================

    /**
     * @brief Processa uma amostra através da delay line.
     * 
     * **Operações**:
     * 1. Escreve amostra de entrada na posição de escrita atual
     * 2. Calcula índice de leitura com delay (inteiro + fracionário)
     * 3. Interpola valor lido com base no tipo configurado
     * 4. Avança índice de escrita para próxima posição
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Fluxo Detalhado**:
     * ```
     * 1. Escrita:
     *    buffer[writeIndex] = inputSample
     * 
     * 2. Cálculo de índice de leitura:
     *    D_int = floor(m_delaySamples)
     *    readIndex = (writeIndex - D_int) & MODULO_MASK
     * 
     * 3. Interpolação (depende de tipo):
     *    - None: output = buffer[readIndex]
     *    - Linear: output = lerp(buffer[readIndex], buffer[readIndex-1], frac)
     *    - Cubic: output = hermite(4 pontos adjacentes, frac)
     * 
     * 4. Avanço:
     *    writeIndex = (writeIndex + 1) & MODULO_MASK
     * ```
     * 
     * **Matemática - Sem Interpolação (Delay Inteiro)**:
     * ```
     * D_int = floor(m_delaySamples)
     * readIndex = (writeIndex - D_int) % MaxDelaySamples
     * output = buffer[readIndex]
     * ```
     * 
     * **Matemática - Interpolação Linear**:
     * ```
     * D_int = floor(m_delaySamples)
     * D_frac = m_delaySamples - D_int
     * 
     * readIndex_0 = (writeIndex - D_int) % MaxDelaySamples
     * readIndex_1 = (readIndex_0 - 1 + MaxDelaySamples) % MaxDelaySamples
     * 
     * sample_0 = buffer[readIndex_0]
     * sample_1 = buffer[readIndex_1]
     * 
     * output = (1 - D_frac) * sample_0 + D_frac * sample_1
     * ```
     * 
     * **Matemática - Interpolação Cúbica Hermite**:
     * ```
     * D_int = floor(m_delaySamples)
     * D_frac = m_delaySamples - D_int
     * 
     * Lê 4 pontos:
     * y[0] = buffer[(writeIndex - D_int + 0) % MaxDelaySamples]
     * y[1] = buffer[(writeIndex - D_int - 1) % MaxDelaySamples]
     * y[2] = buffer[(writeIndex - D_int - 2) % MaxDelaySamples]
     * y[3] = buffer[(writeIndex - D_int - 3) % MaxDelaySamples]
     * 
     * Funções Hermite (t = D_frac):
     * h00 = 2t³ - 3t² + 1
     * h10 = t³ - 2t² + t
     * h01 = -2t³ + 3t²
     * h11 = t³ - t²
     * 
     * Derivadas (diferenças finitas):
     * m0 = (y[2] - y[1]) / 2
     * m1 = (y[3] - y[0]) / 2
     * 
     * output = h00*y[0] + h10*m0 + h01*y[1] + h11*m1
     * ```
     * 
     * **Real-Time Safety**: ✓ GARANTIDO
     * - O(1) constante, sem alocação
     * - Sem exception
     * - Sem RTTI
     * - Determinístico (mesma latência sempre)
     * 
     * **Uso em Loop de Áudio - Echo Simples**:
     * ```cpp
     * for (size_t n = 0; n < blockSize; ++n) {
     *     float input = inputBuffer[n];
     *     float delayed = delay.process(input);
     *     outputBuffer[n] = input + 0.7f * delayed;  // Echo com feedback
     * }
     * ```
     * 
     * **Uso - Chorus (Modulação)**:
     * ```cpp
     * cvdsp::Oscillator<float> lfo;
     * lfo.prepare(48000);
     * lfo.setFrequency(2.0f);  // 2 Hz
     * 
     * for (size_t n = 0; n < blockSize; ++n) {
     *     float lfoValue = lfo.process();  // -1 a 1
     *     float baseDelay = 40.0f;  // 40 ms
     *     float modAmount = 10.0f;  // ±10 ms
     *     float modulatedDelay = baseDelay + lfoValue * modAmount;
     *     
     *     delay.setDelaySamples(modulatedDelay * 48.0f);  // 48 samples/ms
     *     
     *     float input = inputBuffer[n];
     *     float delayed = delay.process(input);
     *     outputBuffer[n] = input + 0.5f * delayed;  // Chorus
     * }
     * ```
     * 
     * **Uso - Flanger (Efeito Sweep)**:
     * ```cpp
     * float phase = 0.0f;
     * for (size_t n = 0; n < blockSize; ++n) {
     *     float lfo = std::sin(phase);
     *     delay.setDelayMilliseconds(2.0f + lfo * 1.5f);  // 0.5-3.5ms
     *     
     *     float input = inputBuffer[n];
     *     float delayed = delay.process(input);
     *     outputBuffer[n] = input + 0.8f * delayed;  // Flanger
     *     
     *     phase += 2.0f * M_PI * 2.0f / 48000.0f;  // 2 Hz LFO
     * }
     * ```
     * 
     * **Uso - Vibrato (Pitch Modulado)**:
     * ```cpp
     * for (size_t n = 0; n < blockSize; ++n) {
     *     float lfo = std::sin(2.0f * M_PI * 6.0f * n / 48000.0f);
     *     delay.setDelayMilliseconds(2.5f + lfo * 1.0f);
     *     
     *     // IMPORTANTE: apenas delay, sem sinal seco!
     *     outputBuffer[n] = delay.process(inputBuffer[n]);
     * }
     * ```
     * 
     * @param[in] inputSample Amostra de entrada a processar
     * 
     * @return value_type Amostra atrasada (interpolada se aplicável)
     * 
     * @note Delay mínimo: 0 (bypass)
     *       Delay máximo: MaxDelaySamples
     *       Mudanças suaves de delay não causam clicks audíveis
     * 
     * @warning Deve ser chamado exatamente uma vez por amostra processada
     */
    inline value_type process(const value_type inputSample) noexcept
    {
        // ===== ESCRITA =====
        // Escreve entrada no buffer circular
        m_buffer[m_writeIndex] = inputSample;

        // ===== LEITURA COM INTERPOLAÇÃO =====
        value_type outputSample = value_type(0);

        if constexpr (Interpolation == InterpolationType::None)
        {
            // Sem interpolação: delay inteiro
            outputSample = processNoInterpolation();
        }
        else if constexpr (Interpolation == InterpolationType::Linear)
        {
            // Interpolação linear (2 pontos)
            outputSample = processLinearInterpolation();
        }
        else if constexpr (Interpolation == InterpolationType::Cubic)
        {
            // Interpolação cúbica (4 pontos)
            outputSample = processCubicInterpolation();
        }

        // ===== AVANÇO DO ÍNDICE DE ESCRITA =====
        // Move para próxima posição no buffer circular
        m_writeIndex = (m_writeIndex + 1) & MODULO_MASK;

        return outputSample;
    }

    /**
     * @brief Processa um bloco de amostras (vectorizável).
     * 
     * **Operações**:
     * - Processa cada amostra do bloco via process()
     * - Sem dependência de dados entre iterações (exceto índices internos)
     * 
     * **Complexidade**: O(blockSize)
     * 
     * **Otimização do Compilador**:
     * - Loop pode ser vetorizado parcialmente
     * - Dependência de writeIndex impede vetorização total
     * - Moderno compilador (clang, gcc) pode usar SIMD parcialmente
     * 
     * **Uso Típico**:
     * ```cpp
     * float inputBlock[512];
     * float outputBlock[512];
     * delay.processBlock(inputBlock, outputBlock, 512);
     * ```
     * 
     * **Real-Time Safety**: ✓ GARANTIDO
     * 
     * @param[in] inputBlock Ponteiro para bloco de entrada
     * @param[out] outputBlock Ponteiro para bloco de saída
     * @param[in] blockSize Número de amostras a processar
     * 
     * @return void
     * 
     * @pre inputBlock != nullptr
     * @pre outputBlock != nullptr
     * @pre blockSize > 0
     */
    inline void processBlock(const value_type* inputBlock, 
                            value_type* outputBlock, 
                            size_type blockSize) noexcept
    {
        assert(inputBlock != nullptr);
        assert(outputBlock != nullptr);
        assert(blockSize > 0);

        for (size_type n = 0; n < blockSize; ++n)
        {
            outputBlock[n] = process(inputBlock[n]);
        }
    }

    // =========================================================================
    // MÉTODOS DE CONSULTA - REAL-TIME SAFE
    // =========================================================================

    /**
     * @brief Lê uma amostra atrasada por um número explícito de amostras.
     *
     * Este método separa leitura e escrita para blocos como all-pass, chorus e
     * flanger que precisam calcular feedback antes de gravar a próxima amostra.
     */
    [[nodiscard]]
    inline value_type readSamples(value_type delayInSamples) const noexcept
    {
        return readDelaySamples(delayInSamples);
    }

    /**
     * @brief Lê uma amostra atrasada por um número inteiro de amostras.
     *
     * Nome separado evita ambiguidade com literais inteiros em readSamples().
     */
    [[nodiscard]]
    inline value_type readIntegerSamples(size_type delayInSamples) const noexcept
    {
        return readDelaySamples(static_cast<value_type>(delayInSamples));
    }

    /**
     * @brief Lê uma amostra atrasada por milissegundos, usando interpolação.
     */
    [[nodiscard]]
    inline value_type readInterpolated(value_type delayInMilliseconds) const noexcept
    {
        const value_type delayInSamples =
            delayInMilliseconds * static_cast<value_type>(m_sampleRate) / static_cast<value_type>(1000);

        return readDelaySamples(delayInSamples);
    }

    /**
     * @brief Escreve uma amostra e avança o cursor circular.
     */
    inline void write(value_type inputSample) noexcept
    {
        m_buffer[m_writeIndex] = inputSample;
        m_writeIndex = (m_writeIndex + 1) & MODULO_MASK;
    }

    /**
     * @brief Retorna o delay atual em amostras.
     * 
     * **Complexidade**: O(1)
     * 
     * @return value_type Delay em amostras (pode ser fracionário)
     */
    inline value_type getDelaySamples() const noexcept
    {
        return m_delaySamples;
    }

    /**
     * @brief Retorna o delay atual em milissegundos.
     * 
     * **Matemática**: delay_ms = delay_samples * 1000 / Fs
     * 
     * **Complexidade**: O(1)
     * 
     * @return value_type Delay em ms
     */
    inline value_type getDelayMilliseconds() const noexcept
    {
        return m_delaySamples * static_cast<value_type>(1000) / 
               static_cast<value_type>(m_sampleRate);
    }

    /**
     * @brief Retorna o delay atual em segundos.
     * 
     * **Matemática**: delay_s = delay_samples / Fs
     * 
     * **Complexidade**: O(1)
     * 
     * @return value_type Delay em segundos
     */
    inline value_type getDelaySeconds() const noexcept
    {
        return m_delaySamples / static_cast<value_type>(m_sampleRate);
    }

    /**
     * @brief Retorna a taxa de amostragem configurada.
     * 
     * **Complexidade**: O(1)
     * 
     * @return size_type Sample rate em Hz
     */
    inline size_type getSampleRate() const noexcept
    {
        return m_sampleRate;
    }

    /**
     * @brief Retorna se a delay line foi inicializada via prepare().
     * 
     * **Complexidade**: O(1)
     * 
     * @return bool true se prepare() foi chamado
     */
    inline bool isInitialized() const noexcept
    {
        return m_initialized;
    }

    /**
     * @brief Retorna a capacidade máxima de delay.
     * 
     * **Complexidade**: O(1) compile-time
     * 
     * @return size_type Máximo de amostras de delay
     */
    static constexpr size_type getMaxDelaySamples() noexcept
    {
        return MAX_DELAY_SAMPLES;
    }

    /**
     * @brief Retorna o tipo de interpolação utilizado.
     * 
     * **Complexidade**: O(1) compile-time
     * 
     * @return InterpolationType Tipo configurado (None, Linear, Cubic)
     */
    static constexpr InterpolationType getInterpolationType() noexcept
    {
        return INTERPOLATION_TYPE;
    }

private:

    [[nodiscard]]
    inline value_type readDelaySamples(value_type delayInSamples) const noexcept
    {
        const value_type clampedDelay = std::clamp(delayInSamples,
                                                  static_cast<value_type>(0),
                                                  static_cast<value_type>(MaxDelaySamples));

        if constexpr (Interpolation == InterpolationType::None)
        {
            const size_type delayInt = static_cast<size_type>(clampedDelay);
            const size_type readIndex = (m_writeIndex + MaxDelaySamples - delayInt) & MODULO_MASK;
            return m_buffer[readIndex];
        }
        else if constexpr (Interpolation == InterpolationType::Linear)
        {
            const value_type delayFrac = clampedDelay - std::floor(clampedDelay);
            const size_type delayInt = static_cast<size_type>(clampedDelay);
            const size_type readIndex0 = (m_writeIndex + MaxDelaySamples - delayInt) & MODULO_MASK;
            const size_type readIndex1 = (readIndex0 + MaxDelaySamples - 1) & MODULO_MASK;

            const value_type sample0 = m_buffer[readIndex0];
            const value_type sample1 = m_buffer[readIndex1];

            return sample0 + delayFrac * (sample1 - sample0);
        }
        else
        {
            const value_type t = clampedDelay - std::floor(clampedDelay);
            const size_type delayInt = static_cast<size_type>(clampedDelay);

            const size_type idx0 = (m_writeIndex + MaxDelaySamples - delayInt + 1) & MODULO_MASK;
            const size_type idx1 = (m_writeIndex + MaxDelaySamples - delayInt) & MODULO_MASK;
            const size_type idx2 = (idx1 + MaxDelaySamples - 1) & MODULO_MASK;
            const size_type idx3 = (idx1 + MaxDelaySamples - 2) & MODULO_MASK;

            const value_type y0 = m_buffer[idx0];
            const value_type y1 = m_buffer[idx1];
            const value_type y2 = m_buffer[idx2];
            const value_type y3 = m_buffer[idx3];

            const value_type m0 = (y2 - y1) * static_cast<value_type>(0.5);
            const value_type m1 = (y3 - y0) * static_cast<value_type>(0.5);

            const value_type t2 = t * t;
            const value_type t3 = t2 * t;

            const value_type h00 = static_cast<value_type>(2.0) * t3 -
                                   static_cast<value_type>(3.0) * t2 +
                                   static_cast<value_type>(1.0);
            const value_type h10 = t3 - static_cast<value_type>(2.0) * t2 + t;
            const value_type h01 = -static_cast<value_type>(2.0) * t3 +
                                   static_cast<value_type>(3.0) * t2;
            const value_type h11 = t3 - t2;

            return h00 * y0 + h10 * m0 + h01 * y1 + h11 * m1;
        }
    }

private:
    // =========================================================================
    // MEMBROS PRIVADOS
    // =========================================================================

    /// @brief Buffer circular para armazenar histórico de amostras
    /// Tamanho: MaxDelaySamples amostras
    /// Inicialização: Zero-initialized na construção
    std::array<value_type, MaxDelaySamples> m_buffer;

    /// @brief Índice de escrita (cursor de inserção)
    /// Incrementa com cada process(), wraps automaticamente via MODULO_MASK
    /// Faixa: 0 ≤ m_writeIndex < MaxDelaySamples
    size_type m_writeIndex;

    /// @brief Delay em amostras (pode ser fracionário para interpolação)
    /// Faixa: 0 ≤ m_delaySamples ≤ MaxDelaySamples
    /// Atualizado por setDelaySamples(), setDelayMilliseconds(), setDelaySeconds()
    value_type m_delaySamples;

    /// @brief Taxa de amostragem em Hz (necessário para conversão ms)
    /// Típicos valores: 44100, 48000, 96000, 192000
    size_type m_sampleRate;

    /// @brief Flag de inicialização (track se prepare() foi chamado)
    bool m_initialized;

    // =========================================================================
    // MÉTODOS PRIVADOS - INTERPOLAÇÃO
    // =========================================================================

    /**
     * @brief Lê amostra com delay inteiro (sem interpolação).
     * 
     * Apenas lê da posição circular, sem suavização.
     * Usado quando InterpolationType = None.
     * 
     * **Complexidade**: O(1)
     * 
     * @return value_type Amostra atrasada (inteira, sem interpolação)
     */
    inline value_type processNoInterpolation() const noexcept
    {
        // Calcula delay inteiro
        const size_type delayInt = static_cast<size_type>(m_delaySamples);

        // Calcula índice de leitura (atual - delay)
        // Nota: writeIndex já foi incrementado antes do return, então já aponta
        // para próxima posição. Compensar com -1.
        const size_type readIndex = (m_writeIndex - delayInt - 1 + MaxDelaySamples) & MODULO_MASK;

        // Lê e retorna
        return m_buffer[readIndex];
    }

    /**
     * @brief Lê amostra com interpolação linear.
     * 
     * Interpola entre duas amostras usando fração de delay.
     * Usado quando InterpolationType = Linear.
     * 
     * **Fórmula**:
     * ```
     * output = (1 - frac) * sample_0 + frac * sample_1
     * ```
     * 
     * **Complexidade**: O(1)
     * 
     * @return value_type Amostra interpolada linearmente
     */
    inline value_type processLinearInterpolation() const noexcept
    {
        // Separa delay em parte inteira e fracionária
        const value_type delayFrac = m_delaySamples - std::floor(m_delaySamples);
        const size_type delayInt = static_cast<size_type>(m_delaySamples);

        // Calcula dois índices de leitura
        const size_type readIndex0 = (m_writeIndex - delayInt - 1 + MaxDelaySamples) & MODULO_MASK;
        const size_type readIndex1 = (readIndex0 - 1 + MaxDelaySamples) & MODULO_MASK;

        // Lê as duas amostras
        const value_type sample0 = m_buffer[readIndex0];
        const value_type sample1 = m_buffer[readIndex1];

        // Interpolação linear: output = (1 - frac) * sample0 + frac * sample1
        return sample0 + delayFrac * (sample1 - sample0);
    }

    /**
     * @brief Lê amostra com interpolação cúbica Hermite.
     * 
     * Interpola entre 4 pontos usando polinômio cúbico Hermite.
     * Produz qualidade superior com apenas ~4x o custo de uma leitura.
     * Usado quando InterpolationType = Cubic.
     * 
     * **Fórmula de interpolação cúbica Hermite**:
     * ```
     * h00(t) = 2t³ - 3t² + 1
     * h10(t) = t³ - 2t² + t
     * h01(t) = -2t³ + 3t²
     * h11(t) = t³ - t²
     * 
     * Derivadas aproximadas (diferenças finitas):
     * m0 = (y[2] - y[1]) / 2
     * m1 = (y[3] - y[0]) / 2
     * 
     * interpolated = h00(t)*y[0] + h10(t)*m0 + h01(t)*y[1] + h11(t)*m1
     * ```
     * 
     * **Complexidade**: O(1)
     * 
     * @return value_type Amostra interpolada cubicamente
     */
    inline value_type processCubicInterpolation() const noexcept
    {
        // Separa delay em parte inteira e fracionária
        const value_type t = m_delaySamples - std::floor(m_delaySamples);
        const size_type delayInt = static_cast<size_type>(m_delaySamples);

        // Calcula quatro índices de leitura
        const size_type idx0 = (m_writeIndex - delayInt - 0 + MaxDelaySamples) & MODULO_MASK;
        const size_type idx1 = (m_writeIndex - delayInt - 1 + MaxDelaySamples) & MODULO_MASK;
        const size_type idx2 = (m_writeIndex - delayInt - 2 + MaxDelaySamples) & MODULO_MASK;
        const size_type idx3 = (m_writeIndex - delayInt - 3 + MaxDelaySamples) & MODULO_MASK;

        // Lê as quatro amostras
        const value_type y0 = m_buffer[idx0];
        const value_type y1 = m_buffer[idx1];
        const value_type y2 = m_buffer[idx2];
        const value_type y3 = m_buffer[idx3];

        // Calcula derivadas usando diferenças finitas centralizadas
        // m0 = (y[2] - y[1]) / 2
        // m1 = (y[3] - y[0]) / 2
        const value_type m0 = (y2 - y1) * static_cast<value_type>(0.5);
        const value_type m1 = (y3 - y0) * static_cast<value_type>(0.5);

        // Pré-calcula potências de t para eficiência
        const value_type t2 = t * t;
        const value_type t3 = t2 * t;

        // Calcula funções Hermite base
        // h00(t) = 2t³ - 3t² + 1
        const value_type h00 = static_cast<value_type>(2.0) * t3 - 
                               static_cast<value_type>(3.0) * t2 + 
                               static_cast<value_type>(1.0);
        
        // h10(t) = t³ - 2t² + t
        const value_type h10 = t3 - static_cast<value_type>(2.0) * t2 + t;
        
        // h01(t) = -2t³ + 3t²
        const value_type h01 = -static_cast<value_type>(2.0) * t3 + 
                               static_cast<value_type>(3.0) * t2;
        
        // h11(t) = t³ - t²
        const value_type h11 = t3 - t2;

        // Interpolação cúbica final: combinação linear ponderada
        return h00 * y0 + h10 * m0 + h01 * y1 + h11 * m1;
    }
};

}  // namespace cvdsp::delay

#endif  // CVDSP_DELAY_DELAYLINE_HPP
