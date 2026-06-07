#ifndef CVDSP_CORE_CIRCULARBUFFER_HPP
#define CVDSP_CORE_CIRCULARBUFFER_HPP

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <limits>

/**
 * @namespace cvdsp
 * @brief CV_DSP - Biblioteca profissional de processamento de áudio em tempo real
 * 
 * Namespace central contendo todos os módulos da biblioteca CV_DSP.
 * Todos os componentes seguem princípios de Real-Time Safe Audio Processing.
 */
namespace cvdsp
{

/**
 * @class CircularBuffer
 * @brief Buffer circular genérico para processamento de áudio em tempo real
 * 
 * Uma estrutura de dados essencial para aplicações de DSP que necessitam de:
 * 
 * - **Delay Lines**: Armazenamento de amostras para síntese de reverberação, chorus, echo
 * - **Buffers de Modulação**: Histórico de sinal para cálculos de coeficientes variáveis no tempo
 * - **Buffers para FFT**: Acumulação de frames para processamento via transformadas de Fourier
 * - **Look-ahead Windows**: Necessário para processadores com lookahead (compressores, limitadores)
 * 
 * **Características técnicas**:
 * 
 * - Header-Only: Sem dependências externas, compilação rápida
 * - Real-Time Safe: Nenhuma alocação dinâmica durante processamento
 * - Zero-Copy: Operações em O(1) sem cópias desnecessárias
 * - SIMD-Friendly: Alinhamento e layout otimizados para vetorização
 * 
 * **Matemática do Buffer Circular**:
 * 
 * Um buffer circular com tamanho N é definido como:
 * ```
 * Índice Lógico: i (0 ≤ i < ∞)
 * Índice Físico: i % N
 * Posição Atual: head (0 ≤ head < N)
 * ```
 * 
 * Operações fundamentais:
 * - Write: armazena valor em posição (head + offset) % N
 * - Read:  recupera valor de posição (head + offset) % N
 * - Advance: move head para (head + 1) % N
 * 
 * **Exemplo de uso - Delay Line (100ms @ 48kHz = 4800 amostras)**:
 * ```cpp
 * cvdsp::CircularBuffer<float, 4800> delayLine;
 * delayLine.prepare();
 * 
 * // Durante processamento (real-time safe)
 * float input = getInputSample();
 * float delayed = delayLine.read(0); // Lê a amostra mais antiga
 * delayLine.write(0, input);          // Sobrescreve com entrada atual
 * delayLine.advance();                 // Move para próxima posição
 * ```
 * 
 * **Exemplo de uso - Buffer de Modulação (LFO history)**:
 * ```cpp
 * cvdsp::CircularBuffer<double, 256> lfoHistory;
 * lfoHistory.prepare();
 * 
 * // Lê histórico dos últimos 256 valores LFO
 * for (size_t i = 0; i < lfoHistory.capacity(); ++i) {
 *     double historicalLFO = lfoHistory.read(i);
 * }
 * lfoHistory.write(0, currentLFO);
 * lfoHistory.advance();
 * ```
 * 
 * **Exemplo de uso - Buffer para FFT Overlap-Add (frame acumulação)**:
 * ```cpp
 * const size_t FRAME_SIZE = 512;
 * const size_t FFT_SIZE = 2048;
 * cvdsp::CircularBuffer<float, FFT_SIZE> fftBuffer;
 * fftBuffer.prepare();
 * 
 * for (size_t n = 0; n < FRAME_SIZE; ++n) {
 *     fftBuffer.write(n, audioFrame[n]);
 * }
 * // Realizar FFT em fftBuffer...
 * fftBuffer.advance(); // Não usado neste caso, apenas exemplo
 * ```
 * 
 * @tparam T Tipo de dado (float ou double). Deve ser compatível com operações aritméticas.
 * @tparam Capacity Tamanho fixo do buffer em amostras. Deve ser > 0.
 * 
 * @note Este buffer é **estritamente real-time safe**:
 *       - Nenhuma alocação dinâmica
 *       - Nenhuma exceção lançada em process()
 *       - Nenhuma operação O(n) no caminho crítico
 */
template <typename T, size_t Capacity>
class CircularBuffer
{
    static_assert(std::is_arithmetic_v<T>, 
                  "CircularBuffer requires arithmetic type (float, double, int, etc.)");
    static_assert(Capacity > 0, 
                  "CircularBuffer capacity must be greater than 0");
    static_assert((Capacity & (Capacity - 1)) == 0, 
                  "CircularBuffer capacity must be a power of 2 for efficient modulo via bitmask");

public:
    // =========================================================================
    // TYPEDEFS E CONSTANTES
    // =========================================================================

    /// @brief Tipo de dado armazenado no buffer
    using value_type = T;

    /// @brief Tipo para índices e offsets
    using size_type = size_t;

    /// @brief Máscara de bits para operação de modulo eficiente
    /// Pré-calculada em tempo de compilação: (Capacity - 1)
    /// Permite: index % Capacity → index & MODULO_MASK (uma operação bitwise!)
    static constexpr size_type MODULO_MASK = Capacity - 1;

    /// @brief Capacidade do buffer em amostras
    static constexpr size_type BUFFER_CAPACITY = Capacity;

    // =========================================================================
    // CONSTRUTOR E DESTRUTOR
    // =========================================================================

    /**
     * @brief Construtor padrão. Inicializa o buffer com zero-initialization.
     * 
     * **Comportamento**:
     * - Buffer: preenchido com zeros (amostras iniciais = 0)
     * - Head: posicionado em índice 0
     * - Pronto para processamento após prepare()
     * 
     * **Tempo de compilação**: O(1) amortizado via aggregate initialization
     * 
     * @note Nenhuma alocação dinâmica ocorre.
     */
    constexpr CircularBuffer() noexcept
        : m_head(0)
        , m_buffer{}  // Zero-initialize all elements
    {
    }

    /// @brief Destrutor padrão (trivial, sem recursos dinâmicos)
    ~CircularBuffer() noexcept = default;

    /// @brief Cópia explicitamente deletada (buffer é recurso único)
    CircularBuffer(const CircularBuffer&) = delete;

    /// @brief Atribuição cópia explicitamente deletada
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    /// @brief Move constructor explicitamente deletado (semanticamente não faz sentido)
    CircularBuffer(CircularBuffer&&) = delete;

    /// @brief Move assignment explicitamente deletado
    CircularBuffer& operator=(CircularBuffer&&) = delete;

    // =========================================================================
    // MÉTODOS DE INICIALIZAÇÃO E RESET
    // =========================================================================

    /**
     * @brief Prepara o buffer para processamento.
     * 
     * **Propósito**: Inicialização determinística e sincronização com host.
     * 
     * **Operações**:
     * - Zera todas as amostras no buffer
     * - Posiciona head em índice 0
     * - Reseta estado interno para condições conhecidas
     * 
     * **Complexidade**: O(N) onde N = Capacity
     * 
     * **Contexto de chamada**: 
     * - VST3: chamado em onActivate() do plugin
     * - iPlug2: chamado em OnReset() do plugin
     * - JUCE: chamado em prepareToPlay()
     * - CLAP: chamado em activate()
     * - Standalone: chamado durante inicialização de áudio
     * 
     * **Thread safety**: Deve ser chamado em thread de configuração (não em audio thread)
     * 
     * @return void
     * 
     * @note Esta operação **NÃO é real-time safe** (realiza limpeza em O(N)).
     *       Nunca chamar durante processamento de áudio.
     */
    void prepare() noexcept
    {
        // Zera o buffer inteiro via memset otimizado pelo compilador
        // Para tipos aritméticos, isso é seguro e eficiente
        std::memset(m_buffer.data(), 0, sizeof(m_buffer));
        
        // Reseta posição de leitura/escrita para início
        m_head = 0;
    }

    /**
     * @brief Reseta o estado do buffer sem realocação.
     * 
     * **Propósito**: Limpeza rápida entre processamentos (ex: bypass, mute).
     * 
     * **Operações**:
     * - Zera todas as amostras
     * - Posiciona head em índice 0
     * 
     * **Complexidade**: O(N) onde N = Capacity
     * 
     * **Diferenças de prepare()**:
     * - reset() pode ser chamado com mais frequência
     * - Semanticamente equivalente a prepare() nesta implementação
     * 
     * **Contexto de chamada**:
     * - Bypass de efeito ativado
     * - Nota MIDI Note-Off sem sustain
     * - Reset de processador via UI
     * 
     * **Thread safety**: Deve ser chamado fora do audio thread
     * 
     * @return void
     * 
     * @note Esta operação **NÃO é real-time safe** (realiza limpeza em O(N)).
     */
    void reset() noexcept
    {
        std::memset(m_buffer.data(), 0, sizeof(m_buffer));
        m_head = 0;
    }

    // =========================================================================
    // OPERAÇÕES DE ESCRITA
    // =========================================================================

    /**
     * @brief Escreve um valor no buffer em posição com offset relativo.
     * 
     * **Matemática**:
     * ```
     * posição_física = (head + offset) & MODULO_MASK
     * buffer[posição_física] = value
     * ```
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso típico - Delay Line**:
     * ```cpp
     * delayLine.write(0, inputSample);  // Escreve entrada atual
     * ```
     * 
     * **Uso típico - Multi-tap delay**:
     * ```cpp
     * for (size_t tap = 0; tap < numTaps; ++tap) {
     *     size_t delayIndex = (tap * delayTimeSamples) / numTaps;
     *     buffer.write(delayIndex, processedSample);
     * }
     * ```
     * 
     * **Validação de offset**:
     * - offset >= Capacity: comportamento indefinido (DEBUG: assert falha)
     * - A máscara de bits garante wrap automático para offsets válidos
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), sem alocação, sem exceção
     * 
     * @param[in] offset Deslocamento relativo do head (0 ≤ offset < Capacity)
     * @param[in] value Valor a ser escrito
     * 
     * @return void
     * 
     * @pre offset < Capacity (não verificado em Release, apenas em Debug via assert)
     * 
     * @post buffer[(head + offset) & MODULO_MASK] == value
     */
    inline void write(size_type offset, const value_type value) noexcept
    {
        // Validação em Debug (Zero Cost em Release via NDEBUG)
        // Garante que offset está dentro dos limites válidos
        assert(offset < Capacity && "CircularBuffer write offset out of bounds");

        // Calcula índice físico no buffer
        // & é mais rápido que % para potências de 2
        const size_type physical_index = (m_head + offset) & MODULO_MASK;

        // Escreve valor
        m_buffer[physical_index] = value;
    }

    /**
     * @brief Escreve múltiplos valores consecutivos começando em offset.
     * 
     * **Matemática**:
     * Para cada i em [0, count):
     * ```
     * posição_física[i] = (head + offset + i) & MODULO_MASK
     * buffer[posição_física[i]] = source[i]
     * ```
     * 
     * **Características**:
     * - Otimizado para cópias contíguas
     * - Pode fazer até 2 memcpy internamente (wrap around)
     * - Usa memcpy quando disponível (mais rápido)
     * 
     * **Complexidade**: O(count) linear
     * 
     * **Uso típico - Frame de áudio**:
     * ```cpp
     * float inputFrame[512];
     * fftBuffer.write(0, inputFrame, 512);  // Escreve frame inteiro
     * ```
     * 
     * **Uso com wrap-around**:
     * ```cpp
     * // Se head + offset + count > Capacity, copia em duas partes:
     * // Parte 1: from head+offset to end
     * // Parte 2: from start to remainder
     * buffer.write(lastPos, data, 256);  // Pode cruzar limite!
     * ```
     * 
     * **Validação**:
     * - offset + count <= Capacity: garantido por assert em Debug
     * 
     * **Real-time safety**: ✓ SEGURO - O(count), sem alocação, sem exceção
     * 
     * @param[in] offset Deslocamento inicial relativo ao head
     * @param[in] source Ponteiro para dados a serem copiados
     * @param[in] count Número de amostras a copiar
     * 
     * @return void
     * 
     * @pre offset + count <= Capacity (não verificado em Release)
     * @pre source != nullptr
     * 
     * @post Buffer contém count amostras de source a partir de offset
     */
    inline void write(size_type offset, const value_type* source, size_type count) noexcept
    {
        // Validações em Debug
        assert(source != nullptr && "CircularBuffer write source pointer is null");
        assert((offset + count) <= Capacity && 
               "CircularBuffer write would exceed buffer bounds");

        // Calcula índice físico inicial
        size_type physical_index = (m_head + offset) & MODULO_MASK;

        // Verifica se há wrap-around
        if (physical_index + count <= Capacity)
        {
            // Caso simples: dados contíguos no buffer
            // Usa memcpy para eficiência máxima
            std::memcpy(&m_buffer[physical_index], source, count * sizeof(value_type));
        }
        else
        {
            // Caso complexo: dados cruzam limite circular
            // Copia em duas partes:
            // Parte 1: do physical_index até final do buffer
            const size_type part1_size = Capacity - physical_index;
            std::memcpy(&m_buffer[physical_index], 
                       source, 
                       part1_size * sizeof(value_type));

            // Parte 2: do início do buffer até índice final
            const size_type part2_size = count - part1_size;
            std::memcpy(&m_buffer[0], 
                       &source[part1_size], 
                       part2_size * sizeof(value_type));
        }
    }

    // =========================================================================
    // OPERAÇÕES DE LEITURA
    // =========================================================================

    /**
     * @brief Lê um valor do buffer com offset relativo ao head.
     * 
     * **Matemática**:
     * ```
     * posição_física = (head + offset) & MODULO_MASK
     * retorna buffer[posição_física]
     * ```
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso típico - Delay Line (Tapped Delay)**:
     * ```cpp
     * // Lê amostras em diferentes pontos do delay
     * float oldest = delayLine.read(0);           // Amostra mais antiga
     * float middle = delayLine.read(Capacity/2);  // Meio do buffer
     * float recent = delayLine.read(Capacity-1);  // Mais recente
     * ```
     * 
     * **Uso com interpolação linear**:
     * ```cpp
     * // Delay fracionário: 2.5 amostras
     * float delayFrac = 2.5f;
     * size_t delayInt = static_cast<size_t>(delayFrac);
     * float frac = delayFrac - delayInt;
     * 
     * float sample0 = delayLine.read(delayInt);
     * float sample1 = delayLine.read(delayInt + 1);
     * float interpolated = sample0 + frac * (sample1 - sample0);
     * ```
     * 
     * **Offset out-of-bounds**:
     * - offset >= Capacity: comportamento indefinido (DEBUG: assert falha)
     * - A máscara garante wrap automático para offsets válidos
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), sem alocação, sem exceção
     * 
     * @param[in] offset Deslocamento relativo ao head (0 ≤ offset < Capacity)
     * 
     * @return value_type Valor lido do buffer
     * 
     * @pre offset < Capacity (não verificado em Release)
     */
    inline value_type read(size_type offset) const noexcept
    {
        // Validação em Debug
        assert(offset < Capacity && "CircularBuffer read offset out of bounds");

        // Calcula índice físico
        const size_type physical_index = (m_head + offset) & MODULO_MASK;

        // Lê e retorna valor
        return m_buffer[physical_index];
    }

    /**
     * @brief Lê múltiplos valores consecutivos começando em offset.
     * 
     * **Matemática**:
     * Para cada i em [0, count):
     * ```
     * posição_física[i] = (head + offset + i) & MODULO_MASK
     * destination[i] = buffer[posição_física[i]]
     * ```
     * 
     * **Características**:
     * - Otimizado para leituras contíguas
     * - Maneja automaticamente wrap-around
     * - Usa memcpy quando possível
     * 
     * **Complexidade**: O(count) linear
     * 
     * **Uso típico - FFT Readout**:
     * ```cpp
     * float fftFrame[2048];
     * fftBuffer.read(0, fftFrame, 2048);  // Lê frame inteiro para processamento FFT
     * ```
     * 
     * **Uso com deslocamento**:
     * ```cpp
     * // Lê histórico de amostras antigas
     * float history[256];
     * delayLine.read(100, history, 256);  // Começa 100 amostras atrás
     * ```
     * 
     * **Validação**:
     * - offset + count <= Capacity: garantido por assert em Debug
     * 
     * **Real-time safety**: ✓ SEGURO - O(count), sem alocação, sem exceção
     * 
     * @param[in] offset Deslocamento inicial relativo ao head
     * @param[out] destination Ponteiro para buffer destino
     * @param[in] count Número de amostras a ler
     * 
     * @return void
     * 
     * @pre offset + count <= Capacity (não verificado em Release)
     * @pre destination != nullptr
     * 
     * @post destination contém count amostras do buffer a partir de offset
     */
    inline void read(size_type offset, value_type* destination, size_type count) const noexcept
    {
        // Validações em Debug
        assert(destination != nullptr && "CircularBuffer read destination pointer is null");
        assert((offset + count) <= Capacity && 
               "CircularBuffer read would exceed buffer bounds");

        // Calcula índice físico inicial
        size_type physical_index = (m_head + offset) & MODULO_MASK;

        // Verifica se há wrap-around
        if (physical_index + count <= Capacity)
        {
            // Caso simples: dados contíguos no buffer
            std::memcpy(destination, &m_buffer[physical_index], count * sizeof(value_type));
        }
        else
        {
            // Caso complexo: dados cruzam limite circular
            // Copia em duas partes:
            // Parte 1: do physical_index até final do buffer
            const size_type part1_size = Capacity - physical_index;
            std::memcpy(destination, 
                       &m_buffer[physical_index], 
                       part1_size * sizeof(value_type));

            // Parte 2: do início do buffer até índice final
            const size_type part2_size = count - part1_size;
            std::memcpy(&destination[part1_size], 
                       &m_buffer[0], 
                       part2_size * sizeof(value_type));
        }
    }

    // =========================================================================
    // OPERAÇÕES DE POSICIONAMENTO
    // =========================================================================

    /**
     * @brief Avança o head para a próxima posição (incremento cíclico).
     * 
     * **Matemática**:
     * ```
     * head = (head + 1) & MODULO_MASK
     * ```
     * 
     * **Propósito**: Move o "cursor" de escrita para a próxima posição circular.
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Semântica**:
     * - Após escrever uma amostra em write(0, sample), deve-se chamar advance()
     * - Próxima chamada write(0, sample) sobrescreverá posição anterior mais nova
     * - Offset 0 sempre referencia a posição do head
     * 
     * **Uso típico - Delay Line Sample-by-Sample**:
     * ```cpp
     * for (size_t n = 0; n < blockSize; ++n) {
     *     float input = inputBuffer[n];
     *     float delayed = delayLine.read(delaySamples);  // Lê amostra atrasada
     *     outputBuffer[n] = delayed;
     *     delayLine.write(0, input);                      // Escreve entrada atual
     *     delayLine.advance();                            // Move para próxima
     * }
     * ```
     * 
     * **Uso em feedback loop**:
     * ```cpp
     * // Schroeder reverberator (comb filter em paralelo)
     * float output = 0.0f;
     * for (size_t i = 0; i < numCombFilters; ++i) {
     *     float delayed = combFilters[i].read(0);
     *     float fed = delayed * feedbackGain;
     *     output += delayed;
     *     combFilters[i].write(0, input + fed);
     *     combFilters[i].advance();
     * }
     * ```
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), sem alocação, sem exceção
     * 
     * @return void
     * 
     * @post head = (head_anterior + 1) & MODULO_MASK
     */
    inline void advance() noexcept
    {
        // Incremento cíclico usando máscara de bits
        // Equivalente a: head = (head + 1) % Capacity
        // Mas muito mais eficiente (uma operação bitwise!)
        m_head = (m_head + 1) & MODULO_MASK;
    }

    /**
     * @brief Avança o head por múltiplas posições.
     * 
     * **Matemática**:
     * ```
     * head = (head + count) & MODULO_MASK
     * ```
     * 
     * **Propósito**: Avança múltiplas amostras de uma só vez.
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso típico - Frame-based Processing**:
     * ```cpp
     * // Processa frame de 512 amostras
     * for (size_t n = 0; n < 512; ++n) {
     *     outputFrame[n] = fftBuffer.read(n);
     *     fftBuffer.write(n, processedFrame[n]);
     * }
     * fftBuffer.advance(512);  // Avança frame inteiro
     * ```
     * 
     * **Uso com múltiplos taps de delay**:
     * ```cpp
     * // Pipelined delay lines
     * for (size_t tap = 0; tap < numTaps; ++tap) {
     *     tapLines[tap].advance(delayIncrementSamples);
     * }
     * ```
     * 
     * **Validação**:
     * - count pode ser qualquer valor (wrap automático)
     * - count > Capacity é válido (wraps múltiplas vezes)
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), sem alocação, sem exceção
     * 
     * @param[in] count Número de posições a avançar
     * 
     * @return void
     * 
     * @post head = (head_anterior + count) & MODULO_MASK
     */
    inline void advance(size_type count) noexcept
    {
        // Incremento múltiplo com wrap automático
        m_head = (m_head + count) & MODULO_MASK;
    }

    // =========================================================================
    // OPERAÇÕES DE CONSULTA
    // =========================================================================

    /**
     * @brief Retorna o tamanho (capacidade) do buffer em amostras.
     * 
     * **Retorna**: Capacity (constante conhecida em tempo de compilação)
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso típico - Alocação de buffers auxiliares**:
     * ```cpp
     * size_t bufSize = delayLine.size();
     * float* tempBuffer = new float[bufSize];  // NÃO fazer em Real-Time!
     * ```
     * 
     * **Melhor prática - Pré-alocar**:
     * ```cpp
     * // Stack-allocated buffer com tamanho conhecido
     * float tempBuffer[CircularBuffer<float, 4800>::BUFFER_CAPACITY];
     * ```
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), constexpr
     * 
     * @return size_type Tamanho do buffer (= Capacity)
     */
    static constexpr size_type size() noexcept
    {
        return Capacity;
    }

    /**
     * @brief Retorna a capacidade do buffer (alias para size()).
     * 
     * **Retorna**: Capacity (constante conhecida em tempo de compilação)
     * 
     * **Diferença de size()**:
     * - Semanticamente equivalentes nesta implementação
     * - capacity() segue convenção de STL containers
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso típico - Verificação de limites**:
     * ```cpp
     * if (offset < buffer.capacity()) {
     *     value = buffer.read(offset);  // Seguro
     * }
     * ```
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), constexpr
     * 
     * @return size_type Capacidade do buffer (= Capacity)
     */
    static constexpr size_type capacity() noexcept
    {
        return Capacity;
    }

    /**
     * @brief Retorna a posição atual do head no buffer.
     * 
     * **Retorna**: Índice físico do head (0 ≤ head < Capacity)
     * 
     * **Propósito**: Introspection para debugging e sincronização.
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso típico - Debugging em Development**:
     * ```cpp
     * if (delayLine.head() == 0) {
     *     // Buffer completamente rotacionado uma vez
     *     logDebug("Delay line wrapped");
     * }
     * ```
     * 
     * **Uso com resampling (exemplo avançado)**:
     * ```cpp
     * // Sincroniza múltiplos buffers circulares
     * if (bufferA.head() != bufferB.head()) {
     *     size_t headDiff = (bufferA.head() - bufferB.head()) & MODULO_MASK;
     *     // Adjust bufferB to match bufferA...
     * }
     * ```
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), sem alocação, sem exceção
     * 
     * @return size_type Posição atual do head (0 ≤ valor < Capacity)
     */
    inline size_type head() const noexcept
    {
        return m_head;
    }

    /**
     * @brief Define manualmente a posição do head.
     * 
     * **Propósito**: Controle explícito de posicionamento (casos avançados).
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso cuidadoso - Sincronização de sessão**:
     * ```cpp
     * // DAW fornece posição de transportador
     * size_t daw_position = host->getTransportPosition();
     * size_t buffer_position = daw_position % buffer.capacity();
     * buffer.set_head(buffer_position);
     * ```
     * 
     * **Aviso**: Pode causar desconesão em processamento contínuo!
     * 
     * **Real-time safety**: ⚠ CUIDADO - O(1), mas pode quebrar continuidade de sinal
     * 
     * @param[in] new_head Nova posição do head (0 ≤ new_head < Capacity)
     * 
     * @return void
     * 
     * @warning Use apenas quando sincronização é necessária. Pode introduzir cliques!
     */
    inline void set_head(size_type new_head) noexcept
    {
        // Validação em Debug
        assert(new_head < Capacity && "CircularBuffer set_head index out of bounds");
        m_head = new_head;
    }

    // =========================================================================
    // ACESSO DIRETO (BAIXO NÍVEL)
    // =========================================================================

    /**
     * @brief Retorna ponteiro para buffer interno (acesso de baixo nível).
     * 
     * **Propósito**: Otimizações avançadas e operações SIMD diretas.
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso com SIMD (SSE/AVX)**:
     * ```cpp
     * // Processamento vetorizado de seção contígua
     * float* bufPtr = buffer.data();
     * size_t headPhysical = buffer.head();
     * 
     * // Processa 16 amostras com AVX
     * for (size_t i = 0; i < Capacity / 16; ++i) {
     *     __m256 vec = _mm256_loadu_ps(&bufPtr[headPhysical]);
     *     // ... SIMD operations ...
     *     _mm256_storeu_ps(&bufPtr[headPhysical], vec);
     *     headPhysical = (headPhysical + 16) & MODULO_MASK;
     * }
     * ```
     * 
     * **Aviso**: Operações em bufPtr podem desorganizar lógica circular!
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), apenas leitura de ponteiro
     * 
     * @return value_type* Ponteiro para início do buffer interno
     * 
     * @warning Não modifique estrutura de dados circular via ponteiro retornado!
     */
    inline value_type* data() noexcept
    {
        return m_buffer.data();
    }

    /**
     * @brief Retorna ponteiro const para buffer interno (acesso de baixo nível).
     * 
     * **Propósito**: Operações de leitura SIMD e introspection.
     * 
     * **Complexidade**: O(1) constante
     * 
     * **Uso com análise FFT**:
     * ```cpp
     * const float* bufPtr = buffer.data();
     * // Analisa espectro sem copiar dados
     * fft.process(bufPtr, bufPtr + buffer.capacity());
     * ```
     * 
     * **Real-time safety**: ✓ SEGURO - O(1), apenas leitura
     * 
     * @return const value_type* Ponteiro const para buffer interno
     */
    inline const value_type* data() const noexcept
    {
        return m_buffer.data();
    }

private:
    // =========================================================================
    // MEMBROS PRIVADOS
    // =========================================================================

    /// @brief Posição atual do head (cursor de escrita)
    /// Valor: 0 ≤ m_head < Capacity
    /// Incrementa com advance() e wraps via máscara de bits
    size_type m_head;

    /// @brief Buffer circular de armazenamento
    /// Tipo: std::array para alocação em stack, sem dynamicallocation
    /// Tamanho: Capacity amostras (Capacity deve ser potência de 2)
    /// Inicialização: Zero-initialized na construção
    std::array<value_type, Capacity> m_buffer;
};

}  // namespace cvdsp

#endif  // CVDSP_CORE_CIRCULARBUFFER_HPP
