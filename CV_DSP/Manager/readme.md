# CV_DSP Manager — Sistema neutro de parâmetros

Este diretório contém a camada de gerenciamento de parâmetros da CV_DSP. Ela foi criada para separar metadados imutáveis, estado runtime, smoothing, automação sample-accurate e snapshots sem depender de SDKs de host.

A implementação atual é formada por três headers principais:

| Arquivo | Papel principal |
| --- | --- |
| `ParameterDescriptor.hpp` | Define metadados imutáveis, IDs, flags, escalas, unidades, ranges, enum entries e conversões `normalized <-> real`. |
| `ParameterState.hpp` | Representa o estado runtime de um parâmetro individual, incluindo valor atual, alvo, dirty flag e smoothing. |
| `ParameterManager.hpp` | Gerencia um conjunto fixo de descriptors/states, lookup por ID, automação sample-accurate e snapshots. |

## Objetivos arquiteturais

- **Header-only**: todos os tipos são definidos diretamente em headers.
- **C++20**: os headers foram escritos para compilação em C++20.
- **Host-neutral**: nenhum arquivo inclui ou referencia Steinberg VST3, JUCE, CLAP ou iPlug2.
- **Real-time safe**: as estruturas evitam alocação dinâmica, exceptions e RTTI.
- **Sem ownership de strings**: nomes, labels e enum entries usam ponteiros não proprietários para storage estável.
- **Separação de responsabilidades**:
  - `ParameterDescriptor` descreve o parâmetro.
  - `ParameterState` armazena e processa o estado runtime.
  - `ParameterManager` organiza múltiplos parâmetros e eventos por bloco.
- **Compatível com adapters futuros**: VST3, JUCE, CLAP, iPlug2 e standalone devem traduzir seus IDs/eventos para o modelo neutro da CV_DSP.

## Namespace

Todos os tipos da camada ficam em:

```cpp
namespace cvdsp::manager
```

Os aliases escalares fundamentais vêm de `CV_DSP/Core/Types.hpp`, como `cvdsp::f32`, `cvdsp::f64`, `cvdsp::u32` e outros.

## Fluxo recomendado de uso

O fluxo normal é:

1. Definir um `ParameterDescriptor<T>` para cada parâmetro.
2. Registrar os descriptors em um `ParameterManager<T, MaxParameters, MaxAutomationEvents>`.
3. Chamar `prepare(...)` com `ProcessContext<T>` ou sample rate/block size.
4. No início de cada bloco, chamar `beginBlock(...)`.
5. Inserir automações do host com `enqueueAutomation(...)`.
6. Avançar os parâmetros por sample com `processSample()` ou por bloco com `processBlockParameters(...)`.
7. Ler valores reais com `getCurrentReal(...)` ou acessar `ParameterState` diretamente.
8. Para presets/state, usar `writeSnapshot(...)` e `applySnapshot(...)`.

## `ParameterDescriptor.hpp`

`ParameterDescriptor<T>` é a descrição imutável de um parâmetro. Ele não armazena estado runtime, smoothing, automação, presets serializados ou objetos de UI/host.

### Tipos principais

#### `ParameterID`

```cpp
using ParameterID = u32;
```

ID neutro e estável usado internamente pela CV_DSP. IDs nativos de hosts, como `Steinberg::Vst::ParamID`, devem ser mapeados por adapters externos.

#### `ParameterFlags`

```cpp
using ParameterFlags = u32;
```

Máscara de bits usada para combinar flags de parâmetro.

#### `ParameterScale`

Define a estratégia de conversão entre valor normalizado e valor real:

| Valor | Uso |
| --- | --- |
| `Linear` | Interpolação linear entre mínimo e máximo. |
| `Logarithmic` | Interpolação em domínio logarítmico; requer range estritamente positivo. |
| `Exponential` | Interpolação com expoente configurável em `ParameterRange::exponent`. |
| `Decibel` | Controle linear em unidade de dB. |
| `Percentage` | Controle percentual baseado em range real `0..1`. |
| `Boolean` | Controle binário thresholded. |
| `Enum` | Controle discreto baseado em lista de opções. |

#### `ParameterUnit`

Hint de unidade para hosts e UIs:

- `None`
- `Hertz`
- `Milliseconds`
- `Seconds`
- `Decibels`
- `Percent`
- `Ratio`
- `Semitones`
- `Cents`
- `Degrees`
- `Samples`
- `Beats`
- `BPM`
- `Index`
- `Custom`

#### `ParameterValueKind`

Categoria de valor inferida pelo descriptor:

- `Continuous`
- `Discrete`
- `Boolean`
- `Enum`

#### `ParameterFlag`

Flags disponíveis:

| Flag | Significado |
| --- | --- |
| `None` | Nenhuma flag. |
| `Automatable` | O host pode automatizar o parâmetro. |
| `Modulatable` | A matriz de modulação futura pode usar o parâmetro como alvo. |
| `Persistent` | Presets/state devem salvar o valor. |
| `Hidden` | Adapters/UI devem ocultar o parâmetro. |
| `ReadOnly` | Parâmetro observável, mas não editável. |
| `Discrete` | Valor real quantizado por step ou índice. |
| `Boolean` | Parâmetro booleano. |
| `Enum` | Parâmetro enumerado. |
| `Bypass` | Parâmetro de bypass. |
| `PerVoice` | Parâmetro endereçável por voz/nota. |
| `Meta` | Parâmetro de metadados, routing ou comportamento auxiliar. |

Funções auxiliares de flags:

- `toMask(flag)`
- `hasFlag(flags, flag)`
- `addFlag(flags, flag)`
- `removeFlag(flags, flag)`
- operadores `|` e `&`
- `ValidParameterFlags`

### `ParameterRange<T>`

Define a faixa numérica real de um parâmetro.

Campos:

| Campo | Descrição |
| --- | --- |
| `minimum` | Valor real mínimo. |
| `maximum` | Valor real máximo. |
| `defaultValue` | Valor real default. |
| `step` | Quantização real; `0` desativa quantização por step. |
| `exponent` | Expoente usado por `ParameterScale::Exponential`. |

Métodos principais:

- `isValid()`
- `isValidForLogarithmic()`
- `clampReal(value)`
- `hasStep()`

Regras de validade:

- Todos os valores precisam ser finitos.
- `maximum` deve ser maior que `minimum`.
- `defaultValue` deve estar dentro do range.
- `step` deve ser maior ou igual a zero.
- `exponent` deve ser positivo.
- Para escala logarítmica, `minimum` e `maximum` precisam ser positivos.

### `ParameterEnumEntry`

Representa uma opção estável de parâmetro enumerado.

Campos:

| Campo | Descrição |
| --- | --- |
| `index` | Índice estável da opção. |
| `label` | Label não proprietário da opção. |

O índice estável não precisa ser igual à posição no array, mas deve ser único dentro da tabela de enum entries.

### `ParameterDescriptor<T>`

#### Responsabilidade

`ParameterDescriptor<T>` centraliza:

- ID neutro.
- Nome curto.
- Nome longo.
- Stable text ID.
- Label de unidade.
- Nome de grupo.
- Unidade.
- Escala.
- Flags.
- Range real.
- Tabela opcional de enum entries.
- Precisão de display.
- Conversões `normalized -> real`.
- Conversões `real -> normalized`.
- Quantização.
- Validação de consistência.

#### Construtores

O construtor principal recebe:

```cpp
ParameterDescriptor(
    ParameterID id,
    const char* shortName,
    const char* longName,
    ParameterUnit unit,
    ParameterScale scale,
    ParameterFlags flags,
    ParameterRange<T> range,
    const ParameterEnumEntry* enumEntries = nullptr,
    std::size_t enumEntryCount = 0,
    const char* stableTextID = nullptr,
    const char* unitLabel = nullptr,
    const char* groupName = nullptr,
    u32 displayPrecision = 2) noexcept;
```

Também existe overload aceitando uma única `ParameterFlag`.

#### Métodos de metadados

- `getID()`
- `getShortName()`
- `getLongName()`
- `getStableTextID()`
- `getUnitLabel()`
- `getGroupName()`
- `getDisplayPrecision()`
- `getUnit()`
- `getScale()`
- `getFlags()`
- `hasFlag(flag)`
- `getRange()`
- `getDefaultValue()`
- `getValueKind()`

#### Métodos para enum

- `getEnumEntries()`
- `getEnumEntryCount()`
- `getEnumEntry(position)`
- `getEnumEntryByStableIndex(index)`
- `normalizedToEnumIndex(normalized)`
- `enumIndexToNormalized(index)`
- `nearestEnumIndex(realValue)`

#### Métodos de conversão e validação

- `isValid()`
- `clampNormalized(value)`
- `clampReal(value)`
- `quantizeReal(value)`
- `normalizedToReal(normalized)`
- `realToNormalized(real)`
- `isAutomatable()`
- `isModulatable()`
- `isPersistent()`

### Exemplo: parâmetro linear

```cpp
using namespace cvdsp::manager;

constexpr ParameterRange<cvdsp::f32> gainRange{
    0.0f,   // minimum
    1.0f,   // maximum
    0.5f,   // defaultValue
    0.0f,   // step
    1.0f    // exponent
};

constexpr ParameterDescriptor<cvdsp::f32> gainDescriptor{
    1,
    "Gain",
    "Output Gain",
    ParameterUnit::Percent,
    ParameterScale::Percentage,
    ParameterFlag::Automatable | ParameterFlag::Persistent,
    gainRange,
    nullptr,
    0,
    "output_gain",
    "%",
    "Output",
    1
};
```

### Exemplo: parâmetro enum

```cpp
using namespace cvdsp::manager;

inline constexpr ParameterEnumEntry modeEntries[] = {
    {0, "Clean"},
    {1, "Crunch"},
    {2, "Lead"}
};

constexpr ParameterRange<cvdsp::f32> modeRange{
    0.0f,
    2.0f,
    0.0f,
    1.0f,
    1.0f
};

constexpr ParameterDescriptor<cvdsp::f32> modeDescriptor{
    2,
    "Mode",
    "Amplifier Mode",
    ParameterUnit::Index,
    ParameterScale::Enum,
    ParameterFlag::Automatable | ParameterFlag::Persistent,
    modeRange,
    modeEntries,
    3,
    "amp_mode",
    nullptr,
    "Amp",
    0
};
```

## `ParameterState.hpp`

`ParameterState<T>` armazena o estado mutável de um único parâmetro. Ele depende de um `ParameterDescriptor<T>` válido e usa os smoothers reais existentes em `CV_DSP/Core/ParameterSmoother.hpp`.

Não existe `ParameterSmoother<T>` genérico. Os modos disponíveis são mapeados para:

| `ParameterSmoothingMode` | Smoother usado |
| --- | --- |
| `None` | Sem smoothing; alvo aplicado imediatamente. |
| `Linear` | `LinearSmoother<T>`. |
| `Exponential` | `ExponentialSmoother<T>`. |
| `OnePole` | `OnePoleSmoother<T>`. |

### `ParameterSmoothingConfig<T>`

Campos:

| Campo | Descrição | Default |
| --- | --- | --- |
| `sampleRate` | Sample rate do host. | `44100` |
| `rampTimeSeconds` | Tempo default de rampa linear/exponencial. | `0.005` |
| `exponentialCurve` | Curvatura para smoothing exponencial. | `5` |
| `onePoleTimeConstant` | Constante de tempo do one-pole. | `0.010` |

### `ParameterState<T>`

#### Responsabilidade

`ParameterState<T>` mantém:

- Ponteiro para descriptor imutável.
- Valor real atual.
- Valor real alvo.
- Valor normalizado atual.
- Valor normalizado alvo.
- Modo de smoothing.
- Configuração sanitizada de smoothing.
- Dirty flag.
- Estado interno dos smoothers concretos.

#### Métodos principais

Binding e preparação:

- `ParameterState()`
- `ParameterState(const descriptor_type* descriptor)`
- `bind(descriptor)`
- `isBound()`
- `isPrepared()`
- `prepare(config)`
- `prepare(sampleRate, rampTimeSeconds, exponentialCurve, onePoleTimeConstant)`

Configuração de smoothing:

- `setSmoothingMode(mode)`
- `getSmoothingMode()`
- `setRampTimeSeconds(seconds)`
- `setExponentialCurve(curve)`
- `setOnePoleTimeConstant(seconds)`

Escrita de valores:

- `resetToDefault()`
- `setImmediateNormalized(normalized)`
- `setImmediateReal(real)`
- `setTargetNormalized(normalized)`
- `setTargetNormalized(normalized, rampSamples)`
- `setTargetReal(real)`
- `setTargetReal(real, rampSamples)`

Processamento:

- `processSample()`

Leitura:

- `getDescriptor()`
- `getID()`
- `getCurrentReal()`
- `getCurrentNormalized()`
- `getTargetReal()`
- `getTargetNormalized()`
- `isSmoothing()`
- `isDirty()`
- `clearDirty()`

Convenience predicates:

- `isAutomatable()`
- `isModulatable()`
- `isPersistent()`
- `isBoolean()`
- `isEnum()`
- `isDiscrete()`

### Exemplo: usar `ParameterState` diretamente

```cpp
using namespace cvdsp::manager;

ParameterState<cvdsp::f32> gainState{&gainDescriptor};

gainState.setSmoothingMode(ParameterSmoothingMode::Linear);

gainState.prepare(ParameterSmoothingConfig<cvdsp::f32>{
    48000.0f,
    0.010f,
    5.0f,
    0.010f
});

gainState.setTargetNormalized(1.0f);

for (std::size_t sample = 0; sample < 64; ++sample)
{
    const cvdsp::f32 gain = gainState.processSample();
    // Use gain no DSP sample-a-sample.
}
```

### Estratégia sample-accurate

Para automação sample-accurate, aplique mudanças exatamente no offset do evento e chame `processSample()` uma vez por sample. `ParameterState` aceita rampas explícitas por número de samples nos overloads com `rampSamples`.

## `ParameterManager.hpp`

`ParameterManager<T, MaxParameters, MaxAutomationEvents>` gerencia múltiplos descriptors e states em arrays de capacidade fixa.

### Template parameters

| Parâmetro | Descrição |
| --- | --- |
| `T` | Tipo de ponto flutuante, normalmente `cvdsp::f32` ou `cvdsp::f64`. |
| `MaxParameters` | Número máximo de parâmetros registrados. |
| `MaxAutomationEvents` | Número máximo de eventos de automação por bloco. |

### Tipos auxiliares

#### `ParameterManagerStatus`

Status retornado por registro e enfileiramento de eventos:

| Status | Significado |
| --- | --- |
| `Ok` | Operação concluída. |
| `Full` | Storage fixo de parâmetros cheio. |
| `InvalidDescriptor` | Descriptor inválido. |
| `DuplicateID` | ID já registrado. |
| `InvalidIndex` | Índice inválido. |
| `InvalidID` | ID não encontrado. |
| `NotAutomatable` | Parâmetro não aceita automação. |
| `EventQueueFull` | Fila fixa de eventos cheia. |
| `InvalidSampleOffset` | Offset fora do bloco atual. |

#### `ParameterAutomationEvent<T>`

Campos:

| Campo | Descrição |
| --- | --- |
| `id` | `ParameterID` neutro. |
| `parameterIndex` | Índice interno resolvido. |
| `sampleOffset` | Offset relativo ao bloco. |
| `normalizedValue` | Valor alvo normalizado. |
| `rampSamples` | Rampa explícita opcional. |
| `hasExplicitRamp` | Indica se `rampSamples` deve ser usado. |

#### `ParameterSnapshotEntry<T>`

Campos:

| Campo | Descrição |
| --- | --- |
| `id` | `ParameterID` neutro. |
| `normalizedValue` | Valor normalizado salvo. |

#### `ParameterSnapshot<T, MaxEntries>`

Snapshot de capacidade fixa.

Campos e métodos:

- `version`
- `count`
- `entries[MaxEntries]`
- `clear()`
- `hasCapacity()`

### Responsabilidades do manager

`ParameterManager` implementa:

- Registro de descriptors.
- Criação e binding de `ParameterState` correspondente.
- Preparação dos states com sample rate e smoothing config.
- Lookup por ID e índice.
- Acesso a descriptor/state.
- Setters imediatos por ID ou índice.
- Enfileiramento de automação sample-accurate.
- Ordenação de eventos por sample offset.
- Aplicação dos eventos no sample correto.
- Processamento sample-a-sample dos states.
- Captura de snapshot para presets/state.
- Aplicação imediata de snapshot.

### Métodos principais

Registro e preparação:

- `registerParameter(descriptor, smoothingMode, smoothingConfig)`
- `prepare(ProcessContext<T>, smoothingConfig)`
- `prepare(sampleRate, maxBlockSize, smoothingConfig)`
- `beginBlock(blockSize)`
- `beginBlock(ProcessContext<T>)`

Lookup e acesso:

- `getParameterCount()`
- `getMaxParameters()`
- `isPrepared()`
- `getCurrentBlockSize()`
- `findIndex(id)`
- `contains(id)`
- `getDescriptorByIndex(index)`
- `getDescriptor(id)`
- `getStateByIndex(index)`
- `getState(id)`

Escrita imediata:

- `setImmediateNormalizedByIndex(index, normalized)`
- `setImmediateNormalized(id, normalized)`
- `setImmediateRealByIndex(index, real)`
- `setImmediateReal(id, real)`

Automação:

- `enqueueAutomation(id, sampleOffset, normalizedValue)`
- `enqueueAutomation(id, sampleOffset, normalizedValue, rampSamples)`
- `enqueueAutomationByIndex(index, sampleOffset, normalizedValue)`
- `enqueueAutomationByIndex(index, sampleOffset, normalizedValue, rampSamples)`
- `clearAutomationEvents()`
- `getAutomationEventCount()`
- `applyAutomationAtSample(sampleIndex)`

Processamento e leitura:

- `processSample()`
- `processBlockParameters(blockSize)`
- `getCurrentRealByIndex(index)`
- `getCurrentReal(id)`
- `getTargetNormalizedByIndex(index)`
- `getTargetNormalized(id)`
- `resetAllToDefaults()`
- `clearAllDirty()`

Snapshots:

- `writeSnapshot(snapshot, persistentOnly)`
- `applySnapshot(snapshot)`

### Exemplo completo com `ParameterManager`

```cpp
#include "CV_DSP/Manager/ParameterManager.hpp"

using namespace cvdsp::manager;

constexpr ParameterRange<cvdsp::f32> gainRange{
    0.0f,
    1.0f,
    0.5f,
    0.0f,
    1.0f
};

constexpr ParameterDescriptor<cvdsp::f32> gainDescriptor{
    1,
    "Gain",
    "Output Gain",
    ParameterUnit::Percent,
    ParameterScale::Percentage,
    ParameterFlag::Automatable | ParameterFlag::Persistent,
    gainRange,
    nullptr,
    0,
    "output_gain",
    "%",
    "Output",
    1
};

ParameterManager<cvdsp::f32, 16, 128> parameters;

void prepareParameters(cvdsp::f32 sampleRate, std::size_t blockSize)
{
    ParameterSmoothingConfig<cvdsp::f32> smoothing;
    smoothing.sampleRate = sampleRate;
    smoothing.rampTimeSeconds = 0.010f;
    smoothing.exponentialCurve = 5.0f;
    smoothing.onePoleTimeConstant = 0.010f;

    parameters.registerParameter(
        gainDescriptor,
        ParameterSmoothingMode::Linear,
        smoothing);

    parameters.prepare(sampleRate, blockSize, smoothing);
}

void processBlock(std::size_t blockSize)
{
    parameters.beginBlock(blockSize);

    // Exemplo de automação host-neutral:
    parameters.enqueueAutomation(1, 0, 0.25f);
    parameters.enqueueAutomation(1, blockSize / 2, 0.75f, 32);

    for (std::size_t sample = 0; sample < blockSize; ++sample)
    {
        parameters.processSample();
        const cvdsp::f32 gain = parameters.getCurrentReal(1);
        // Aplicar gain no DSP deste sample.
    }
}
```

## Presets e state

A camada atual não serializa bytes, JSON, XML ou `IBStream`. Ela fornece uma representação neutra em memória para ser usada por `PresetManager` ou adapters futuros.

### Capturar snapshot

```cpp
ParameterSnapshot<cvdsp::f32, 64> snapshot;

const bool ok = parameters.writeSnapshot(snapshot, true);
```

Com `persistentOnly = true`, apenas parâmetros com `ParameterFlag::Persistent` são incluídos.

### Aplicar snapshot

```cpp
parameters.applySnapshot(snapshot);
```

A aplicação usa valores normalizados e chama setters imediatos. Isso é adequado para restauração de preset/state antes de processar áudio ou em pontos seguros de sincronização.

## Automação sample-accurate

A automação deve chegar ao manager já traduzida para:

- `ParameterID` neutro ou índice interno resolvido.
- `sampleOffset` relativo ao bloco atual.
- `normalizedValue` no intervalo `0..1`.
- `rampSamples` opcional.

Fluxo recomendado por bloco:

```cpp
parameters.beginBlock(blockSize);

// Adapter do host insere eventos em ordem qualquer.
parameters.enqueueAutomation(parameterID, sampleOffset, normalizedValue);

for (std::size_t sample = 0; sample < blockSize; ++sample)
{
    parameters.processSample();
    // Ler valores atuais e processar DSP.
}
```

`ParameterManager` ordena a fila por `sampleOffset` e aplica todos os eventos cujo offset é menor ou igual ao sample processado.

## Compatibilidade com hosts

### VST3

Um adapter VST3 futuro deve:

- Mapear `Steinberg::Vst::ParamID` para `ParameterID`.
- Converter `IParamValueQueue` em chamadas `enqueueAutomation(...)`.
- Converter valores VST3 normalizados diretamente para `normalizedValue`.
- Converter state via `IBStream` para/desde `ParameterSnapshot` ou `PresetManager`.
- Manter qualquer include de Steinberg fora de `CV_DSP/Manager`.

### JUCE

Um adapter JUCE futuro deve:

- Mapear `juce::AudioProcessorParameter` ou `AudioProcessorValueTreeState` para `ParameterDescriptor`.
- Traduzir valores normalizados para `setImmediateNormalized(...)` ou `enqueueAutomation(...)`.
- Serializar presets usando `ParameterSnapshot` como formato intermediário neutro.
- Manter includes de JUCE fora de `CV_DSP/Manager`.

### CLAP

Um adapter CLAP futuro deve:

- Mapear `clap_id` para `ParameterID`.
- Traduzir eventos `CLAP_EVENT_PARAM_VALUE` e ramps para a fila do manager.
- Usar flags do descriptor para capacidades de automação/modulação/persistência.
- Manter includes de CLAP fora de `CV_DSP/Manager`.

### iPlug2

Um adapter iPlug2 futuro deve:

- Mapear IDs de parâmetro iPlug2 para `ParameterID`.
- Alimentar `ParameterManager` com mudanças normalizadas.
- Usar descriptors para construir metadados de UI/host.
- Manter includes de iPlug2 fora de `CV_DSP/Manager`.

### Standalone

Aplicações standalone podem usar diretamente:

- `setImmediateNormalized(...)` para sliders/UI.
- `setTargetNormalized(...)` em `ParameterState` para transições suaves.
- `ParameterSnapshot` para salvar/restaurar configurações.

## Boas práticas

- Use IDs estáveis e nunca reutilize um `ParameterID` para outro significado.
- Prefira strings e arrays `static`, `constexpr` ou `inline constexpr`, pois descriptors não copiam nem possuem strings.
- Marque parâmetros salvos em preset com `ParameterFlag::Persistent`.
- Marque parâmetros automatizáveis com `ParameterFlag::Automatable`; caso contrário, `ParameterManager` rejeita eventos de automação.
- Use `ParameterFlag::Modulatable` somente para parâmetros que poderão ser alvo da futura `ModulationMatrix`.
- Evite chamar `registerParameter(...)` dentro do callback de áudio; registre durante setup/prepare.
- Chame `beginBlock(...)` antes de inserir automações de um novo bloco.
- Chame `processSample()` exatamente uma vez por sample se precisar de automação sample-accurate.
- Use `getCurrentReal(...)` para valores consumidos pelo DSP.
- Use `getTargetNormalized(...)` para valores destinados a host/preset.
- Use snapshots em pontos seguros de sincronização, não como mecanismo de automação por sample.

## Limitações intencionais

- Não há serialização para arquivo ou stream nesta camada.
- Não há dependência de host SDK.
- Não há alocação dinâmica para listas de parâmetros, eventos ou snapshots.
- Não há busca hash/map dinâmica; lookup por ID é linear para preservar simplicidade e evitar heap.
- Não há ownership de nomes, labels ou tabelas de enum.
- Não há sistema de modulação ainda; apenas flags e compatibilidade futura.
- Não há gerenciamento de vozes ainda; `ParameterFlag::PerVoice` é metadado para integração futura.

## Relação entre os três arquivos

```text
ParameterDescriptor<T>
    ↓ descreve
ParameterState<T>
    ↓ armazena estado runtime e smoothing
ParameterManager<T, MaxParameters, MaxAutomationEvents>
    ↓ organiza múltiplos parâmetros, automação e snapshots
Adapters futuros
    ↓ traduzem SDKs/hosts para o modelo neutro
VST3 / JUCE / CLAP / iPlug2 / Standalone
```

Essa divisão evita duplicação com `Core`, mantém `Manager` independente de frameworks e cria uma base estável para `PresetManager`, `ModulationMatrix`, `VoiceManager` e adapters em `CV_DSP/Adapters/`.

---

# CV_DSP Adapters/VST3 — Contexto e buffers de áudio

Além da camada `Manager`, a biblioteca agora possui adapters VST3 iniciais em:

```text
CV_DSP/Adapters/VST3/
├── VST3ProcessContextAdapter.hpp
├── VST3AudioBufferAdapter.hpp
└── VST3ParameterAdapter.hpp
```

Esses arquivos não fazem parte do `Core` nem do `Manager`. Eles são tradutores finos entre o SDK Steinberg VST3 e os tipos neutros já existentes da CV_DSP.

| Arquivo | Tradução principal |
| --- | --- |
| `VST3ProcessContextAdapter.hpp` | `Steinberg::Vst::ProcessData` / `Steinberg::Vst::ProcessContext` → `cvdsp::ProcessContext<T>`. |
| `VST3AudioBufferAdapter.hpp` | `Steinberg::Vst::AudioBusBuffers` → `cvdsp::AudioBufferView<T>` / `cvdsp::ConstAudioBufferView<T>`. |
| `VST3ParameterAdapter.hpp` | `Steinberg::Vst::IParameterChanges` / `IParamValueQueue` → `ParameterManager::enqueueAutomation(...)`. |

## Objetivos dos adapters VST3

- Manter `Core` e `Manager` independentes do Steinberg VST3 SDK.
- Evitar duplicação de `AudioBufferView`, `ProcessContext`, `ParameterManager` ou smoothing.
- Usar operação **zero-copy** para áudio.
- Não possuir memória de host.
- Não alocar dinamicamente.
- Não lançar exceptions.
- Não usar RTTI.
- Serem headers C++17-compatible, stateless e real-time safe.
- Traduzir somente o necessário para o modelo neutro da CV_DSP.

## Namespace dos adapters VST3

Os adapters VST3 ficam em:

```cpp
namespace cvdsp::adapters::vst3
```

Uso típico:

```cpp
using namespace cvdsp::adapters::vst3;
```

## Dependências dos adapters VST3

### `VST3ProcessContextAdapter.hpp`

Inclui:

```cpp
#include "../../Core/ProcessContext.hpp"
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
```

### `VST3AudioBufferAdapter.hpp`

Inclui:

```cpp
#include "../../Core/AudioBufferView.hpp"
#include <cstddef>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
```

`VST3ParameterAdapter.hpp` inclui:

```cpp
#include "../../Manager/ParameterManager.hpp"
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
```

Nenhum dos adapters inclui:

- `PresetManager`
- `ModulationMatrix`
- `VoiceManager`
- JUCE
- CLAP
- iPlug2

---

## `VST3ProcessContextAdapter.hpp`

`VST3ProcessContextAdapter` converte informações de timing/transporte do VST3 para `cvdsp::ProcessContext<T>`.

### Responsabilidade

Traduzir:

```text
Steinberg::Vst::ProcessData
Steinberg::Vst::ProcessContext
```

para:

```text
cvdsp::ProcessContext<float>
cvdsp::ProcessContext<double>
```

Ele não adapta áudio, parâmetros, MIDI/eventos, presets, modulação ou vozes.

### Características

- Classe `final` com construtor, cópia, assignment e destrutor deletados.
- Uso exclusivamente estático.
- Aceita `ProcessContext` VST3 nulo.
- Aplica defaults seguros antes de ler o contexto VST3.
- Respeita flags de validade do VST3 para campos opcionais.
- Retorna `bool` nas funções `fill...` indicando se o `cvdsp::ProcessContext<T>` final é válido.

### API pública

#### Criar contexto a partir de `ProcessData`

```cpp
static cvdsp::ProcessContext<T> fromProcessData(
    const Steinberg::Vst::ProcessData& data,
    T fallbackSampleRate,
    std::size_t numChannels,
    T fallbackTempo = static_cast<T>(120)) noexcept;
```

Uso:

```cpp
const auto context = VST3ProcessContextAdapter::fromProcessData<float>(
    data,
    48000.0f,
    2);
```

#### Preencher contexto existente a partir de `ProcessData`

```cpp
static bool fillFromProcessData(
    cvdsp::ProcessContext<T>& destination,
    const Steinberg::Vst::ProcessData& data,
    T fallbackSampleRate,
    std::size_t numChannels,
    T fallbackTempo = static_cast<T>(120)) noexcept;
```

Uso:

```cpp
cvdsp::ProcessContext<float> context;

const bool valid = VST3ProcessContextAdapter::fillFromProcessData(
    context,
    data,
    48000.0f,
    2);
```

#### Criar contexto a partir de `ProcessContext` VST3

```cpp
static cvdsp::ProcessContext<T> fromProcessContext(
    const Steinberg::Vst::ProcessContext* vstContext,
    std::size_t blockSize,
    std::size_t numChannels,
    T fallbackSampleRate,
    T fallbackTempo = static_cast<T>(120)) noexcept;
```

Também existe overload por referência:

```cpp
static cvdsp::ProcessContext<T> fromProcessContext(
    const Steinberg::Vst::ProcessContext& vstContext,
    std::size_t blockSize,
    std::size_t numChannels,
    T fallbackSampleRate,
    T fallbackTempo = static_cast<T>(120)) noexcept;
```

#### Preencher contexto existente a partir de `ProcessContext` VST3

```cpp
static bool fillFromProcessContext(
    cvdsp::ProcessContext<T>& destination,
    const Steinberg::Vst::ProcessContext* vstContext,
    std::size_t blockSize,
    std::size_t numChannels,
    T fallbackSampleRate,
    T fallbackTempo = static_cast<T>(120)) noexcept;
```

Também existe overload por referência.

### Campos convertidos

| VST3 | CV_DSP | Regra |
| --- | --- | --- |
| `ProcessData::numSamples` | `blockSize` | Valores negativos viram `0`. |
| `ProcessContext::sampleRate` | `sampleRate` | Copiado se positivo; senão usa fallback. |
| `ProcessContext::tempo` | `tempo` | Copiado somente com `kTempoValid` e valor positivo. |
| `timeSigNumerator` / `timeSigDenominator` | `timeSignatureNumerator` / `timeSignatureDenominator` | Copiados somente com `kTimeSigValid` e valores positivos. |
| `state & kPlaying` | `isPlaying` | Conversão direta para bool. |
| `state & kRecording` | `isRecording` | Conversão direta para bool. |
| `projectTimeSamples` | `samplePosition` | Copiado somente se não negativo. |
| `projectTimeMusic` | `ppqPosition` | Copiado somente com `kProjectTimeMusicValid`. |
| `barPositionMusic` | `barStartPPQ` | Copiado somente com `kBarPositionValid`. |
| `projectTimeSamples / sampleRate` | `timeInSeconds` | Preferido quando `projectTimeSamples` é não negativo. |
| `continousTimeSamples / sampleRate` | `timeInSeconds` | Fallback quando `kContTimeValid` está ativo. |

### Defaults seguros

Antes de aplicar o VST3, o adapter chama/reset logic equivalente ao default de `ProcessContext<T>` e define:

| Campo | Default/fallback |
| --- | --- |
| `sampleRate` | `fallbackSampleRate` se positivo; senão `44100`. |
| `blockSize` | `ProcessData::numSamples` convertido para `std::size_t`, ou `0`. |
| `numChannels` | Valor informado pelo caller. |
| `tempo` | `fallbackTempo` se positivo; senão `120`. |
| `timeSignatureNumerator` | `4`. |
| `timeSignatureDenominator` | `4`. |
| `isPlaying` | `false`. |
| `isRecording` | `false`. |
| `samplePosition` | `0`. |
| `ppqPosition` | `0`. |
| `barStartPPQ` | `0`. |
| `timeInSeconds` | `0`. |

### Tutorial: usar no `process()` VST3

```cpp
#include "CV_DSP/Adapters/VST3/VST3ProcessContextAdapter.hpp"

using cvdsp::adapters::vst3::VST3ProcessContextAdapter;

void processVST3Block(
    Steinberg::Vst::ProcessData& data,
    double sampleRate,
    std::size_t mainBusChannels)
{
    auto context = VST3ProcessContextAdapter::fromProcessData<float>(
        data,
        static_cast<float>(sampleRate),
        mainBusChannels);

    if (!context.isValid())
    {
        return;
    }

    // context.sampleRate
    // context.blockSize
    // context.tempo
    // context.ppqPosition
    // context.isPlaying
    // ... podem ser usados pelos DSPs CV_DSP.
}
```

### Cuidados

- `ProcessData::numInputs` e `numOutputs` são contagens de buses, não canais.
- `numChannels` deve vir do bus principal, do audio adapter ou da política do plugin.
- Campos musicais VST3 sem flag válida não são confiáveis.
- `systemTime` VST3 não é usado diretamente como `timeInSeconds`.
- `projectTimeSamples` negativo é ignorado para evitar conversão perigosa para `std::uint64_t`.

---

## `VST3AudioBufferAdapter.hpp`

`VST3AudioBufferAdapter` converte `Steinberg::Vst::AudioBusBuffers` para views de áudio neutras da CV_DSP.

### Responsabilidade

Traduzir:

```text
Steinberg::Vst::AudioBusBuffers::channelBuffers32
Steinberg::Vst::AudioBusBuffers::channelBuffers64
```

para:

```text
cvdsp::AudioBufferView<float>
cvdsp::AudioBufferView<double>
cvdsp::ConstAudioBufferView<float>
cvdsp::ConstAudioBufferView<double>
```

### Características

- Classe `final` com construtor, cópia, assignment e destrutor deletados.
- Uso exclusivamente estático.
- Zero-copy: não copia samples.
- Não aloca buffers.
- Não possui memória do host.
- Valida `numSamples`, `numChannels`, array de canais e ponteiros individuais.
- Retorna view vazia/default quando o bus não pode ser usado.
- Inclui helpers para `silenceFlags`.

### API pública de criação de views

#### Mutable 32-bit

```cpp
static cvdsp::AudioBufferView<float> makeMutable32(
    Steinberg::Vst::AudioBusBuffers& bus,
    std::size_t numSamples) noexcept;
```

Cria view mutável sobre `channelBuffers32`.

#### Mutable 64-bit

```cpp
static cvdsp::AudioBufferView<double> makeMutable64(
    Steinberg::Vst::AudioBusBuffers& bus,
    std::size_t numSamples) noexcept;
```

Cria view mutável sobre `channelBuffers64`.

#### Const 32-bit

```cpp
static cvdsp::ConstAudioBufferView<float> makeConst32(
    const Steinberg::Vst::AudioBusBuffers& bus,
    std::size_t numSamples) noexcept;
```

Cria view const sobre `channelBuffers32`.

#### Const 64-bit

```cpp
static cvdsp::ConstAudioBufferView<double> makeConst64(
    const Steinberg::Vst::AudioBusBuffers& bus,
    std::size_t numSamples) noexcept;
```

Cria view const sobre `channelBuffers64`.

### API pública de validação

```cpp
static std::size_t sampleCount(Steinberg::int32 numSamples) noexcept;
static std::size_t channelCount(const Steinberg::Vst::AudioBusBuffers& bus) noexcept;
static bool hasChannels(const Steinberg::Vst::AudioBusBuffers& bus) noexcept;
static bool isValid32(const Steinberg::Vst::AudioBusBuffers& bus, std::size_t numSamples) noexcept;
static bool isValid64(const Steinberg::Vst::AudioBusBuffers& bus, std::size_t numSamples) noexcept;
static bool isValid32(const Steinberg::Vst::AudioBusBuffers& bus, Steinberg::int32 numSamples) noexcept;
static bool isValid64(const Steinberg::Vst::AudioBusBuffers& bus, Steinberg::int32 numSamples) noexcept;
static bool isInactive(const Steinberg::Vst::AudioBusBuffers& bus, std::size_t numSamples) noexcept;
```

Regras de validação:

- `numSamples` precisa ser maior que zero.
- `numChannels` precisa ser maior que zero.
- `channelBuffers32` ou `channelBuffers64` precisa ser não nulo.
- Todos os ponteiros individuais de canais precisam ser não nulos.
- Se qualquer condição falhar, o adapter retorna view vazia/default.

### API pública para `silenceFlags`

```cpp
static bool isChannelSilent(const Steinberg::Vst::AudioBusBuffers& bus, std::size_t channel) noexcept;
static bool isBusSilent(const Steinberg::Vst::AudioBusBuffers& bus) noexcept;
static void markChannelSilent(Steinberg::Vst::AudioBusBuffers& bus, std::size_t channel) noexcept;
static void clearChannelSilent(Steinberg::Vst::AudioBusBuffers& bus, std::size_t channel) noexcept;
static void markAllChannelsSilent(Steinberg::Vst::AudioBusBuffers& bus) noexcept;
static void clearAllSilenceFlags(Steinberg::Vst::AudioBusBuffers& bus) noexcept;
static Steinberg::uint64 silenceFlags(const Steinberg::Vst::AudioBusBuffers& bus) noexcept;
```

### Tutorial: criar views de input/output 32-bit

```cpp
#include "CV_DSP/Adapters/VST3/VST3AudioBufferAdapter.hpp"

using cvdsp::adapters::vst3::VST3AudioBufferAdapter;

void processFloatBus(Steinberg::Vst::ProcessData& data)
{
    const auto samples = VST3AudioBufferAdapter::sampleCount(data.numSamples);

    if (data.numInputs <= 0 || data.numOutputs <= 0)
        return;

    auto input = VST3AudioBufferAdapter::makeConst32(
        data.inputs[0],
        samples);

    auto output = VST3AudioBufferAdapter::makeMutable32(
        data.outputs[0],
        samples);

    if (!input.isValid() || !output.isValid())
        return;

    const std::size_t channels = output.getNumChannels();
    const std::size_t numSamples = output.getNumSamples();

    for (std::size_t channel = 0; channel < channels; ++channel)
    {
        const float* in = input.getChannel(channel);
        float* out = output.getChannel(channel);

        for (std::size_t sample = 0; sample < numSamples; ++sample)
            out[sample] = in[sample];
    }

    VST3AudioBufferAdapter::clearAllSilenceFlags(data.outputs[0]);
}
```

### Tutorial: criar views de output 64-bit

```cpp
using cvdsp::adapters::vst3::VST3AudioBufferAdapter;

void processDoubleOutput(
    Steinberg::Vst::AudioBusBuffers& outputBus,
    Steinberg::int32 vstNumSamples)
{
    auto output = VST3AudioBufferAdapter::makeMutable64(
        outputBus,
        VST3AudioBufferAdapter::sampleCount(vstNumSamples));

    if (!output.isValid())
        return;

    for (std::size_t channel = 0; channel < output.getNumChannels(); ++channel)
    {
        double* out = output.getChannel(channel);

        for (std::size_t sample = 0; sample < output.getNumSamples(); ++sample)
            out[sample] = 0.0;
    }

    VST3AudioBufferAdapter::markAllChannelsSilent(outputBus);
}
```

### Estratégia para buses inativos

Um bus é tratado como sem áudio útil quando:

- `numSamples == 0`;
- `numChannels <= 0`;
- array de canais é nulo;
- qualquer ponteiro individual de canal é nulo.

Nesses casos, o adapter não lança erro e não aloca fallback. Ele apenas retorna view vazia/default.

### Estratégia para `silentFlags`

- `silentFlags` são tratados como hints de otimização.
- Input silencioso não significa automaticamente que o plugin não deve processar.
- Output deve limpar flags quando gerar áudio.
- Output deve marcar flags quando realmente gerar silêncio.
- As flags não substituem validação de ponteiros.

### Lifetime das views

As views retornadas por `VST3AudioBufferAdapter` são não proprietárias.

Regra obrigatória:

```text
Criar view dentro da chamada process().
Usar imediatamente.
Descartar antes de retornar ao host.
Nunca armazenar como membro persistente.
```

O host VST3 continua sendo dono dos buffers.

---

## `VST3ParameterAdapter.hpp`

`VST3ParameterAdapter` converte automação de parâmetros VST3 para a fila neutra já existente do `ParameterManager`.

### Responsabilidade

Traduzir:

```text
Steinberg::Vst::ProcessData::inputParameterChanges
Steinberg::Vst::IParameterChanges
Steinberg::Vst::IParamValueQueue
```

para chamadas diretas a:

```cpp
ParameterManager::enqueueAutomation(parameterID, sampleOffset, normalizedValue)
```

Ele não cria fila própria, não interpola curvas, não aplica smoothing e não converte automação para valor real antes do Manager/State.

### Tipos principais

#### `VST3IdentityParameterIDMapper`

Mapper default que converte `Steinberg::Vst::ParamID` para `cvdsp::manager::ParameterID` usando o mesmo valor numérico.

```cpp
VST3IdentityParameterIDMapper mapper;
cvdsp::manager::ParameterID id = 0;
mapper.map(vstParamID, id);
```

Hosts/wrappers podem fornecer um mapper customizado com a mesma API:

```cpp
struct MyMapper
{
    bool map(Steinberg::Vst::ParamID vstID,
             cvdsp::manager::ParameterID& outID) const noexcept;
};
```

#### `VST3ParameterAdapterResult`

Resultado diagnóstico sem alocação usado para auditoria e debugging fora do caminho crítico:

| Campo | Significado |
| --- | --- |
| `queuesVisited` | Quantidade de queues VST3 visitadas. |
| `queuesRejected` | Queues rejeitadas antes de percorrer pontos. |
| `pointsVisited` | Pontos VST3 visitados. |
| `pointsEnqueued` | Pontos aceitos pelo `ParameterManager`. |
| `pointsRejected` | Pontos rejeitados pelo adapter ou manager. |
| `invalidQueues` | Queues nulas/inválidas. |
| `invalidPoints` | Pontos inválidos ou `getPoint` sem sucesso. |
| `invalidIDs` | IDs mapeados que não existem no Manager. |
| `notAutomatable` | Pontos rejeitados por parâmetro não automatizável. |
| `invalidSampleOffsets` | Offsets fora do bloco atual. |
| `queueFull` | Rejeições por fila fixa cheia no Manager. |
| `lastStatus` | Último `ParameterManagerStatus` observado. |
| `hadOverflow` | `true` quando `EventQueueFull` ocorreu. |

`success()` retorna `true` quando nenhuma rejeição foi registrada.

### API pública

Validação:

```cpp
static bool isValidChanges(Steinberg::Vst::IParameterChanges* changes) noexcept;
static bool isValidQueue(const Steinberg::Vst::IParamValueQueue* queue) noexcept;
static bool hasPoints(Steinberg::int32 pointCount) noexcept;
static bool isValidSampleOffset(Steinberg::int32 sampleOffset) noexcept;
static std::size_t toSampleOffset(Steinberg::int32 sampleOffset) noexcept;
```

Adaptação de `ProcessData`:

```cpp
static VST3ParameterAdapterResult adaptProcessData(
    const Steinberg::Vst::ProcessData& data,
    ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager) noexcept;

static VST3ParameterAdapterResult adaptProcessData(
    const Steinberg::Vst::ProcessData& data,
    ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
    const Mapper& mapper) noexcept;
```

Adaptação de `IParameterChanges`:

```cpp
static VST3ParameterAdapterResult adaptParameterChanges(
    Steinberg::Vst::IParameterChanges* changes,
    ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager) noexcept;

static VST3ParameterAdapterResult adaptParameterChanges(
    Steinberg::Vst::IParameterChanges* changes,
    ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
    const Mapper& mapper) noexcept;
```

Adaptação de uma queue:

```cpp
static VST3ParameterAdapterResult adaptQueue(
    Steinberg::Vst::IParamValueQueue& queue,
    ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager) noexcept;

static VST3ParameterAdapterResult adaptQueue(
    Steinberg::Vst::IParamValueQueue& queue,
    ParameterManager<T, MaxParameters, MaxAutomationEvents>& manager,
    const Mapper& mapper) noexcept;
```

### Tutorial: adaptar automação VST3 para `ParameterManager`

```cpp
#include "CV_DSP/Adapters/VST3/VST3ParameterAdapter.hpp"

using cvdsp::adapters::vst3::VST3ParameterAdapter;

void processAutomation(
    Steinberg::Vst::ProcessData& data,
    cvdsp::manager::ParameterManager<float, 128, 1024>& parameters,
    std::size_t blockSize)
{
    // Importante: beginBlock limpa a fila de automação e define o tamanho do bloco.
    parameters.beginBlock(blockSize);

    const auto result = VST3ParameterAdapter::adaptProcessData(
        data,
        parameters);

    if (result.hadOverflow)
    {
        // Diagnóstico opcional fora do caminho crítico.
    }
}
```

### Tutorial: mapper customizado

```cpp
struct ParamMapper
{
    bool map(
        Steinberg::Vst::ParamID vstID,
        cvdsp::manager::ParameterID& outID) const noexcept
    {
        switch (vstID)
        {
            case 1000: outID = 1; return true;
            case 1001: outID = 2; return true;
            default: return false;
        }
    }
};

const auto result = VST3ParameterAdapter::adaptProcessData(
    data,
    parameters,
    ParamMapper{});
```

### Estratégia sample-accurate

- O adapter preserva `sampleOffset` retornado por `IParamValueQueue::getPoint`.
- O adapter preserva `ParamValue` normalizado.
- O adapter encaminha cada ponto válido para `ParameterManager::enqueueAutomation(...)`.
- O Manager ordena/aplica os eventos e o `ParameterState` faz smoothing, se configurado.
- O adapter para no primeiro `EventQueueFull` e registra `hadOverflow`, sem criar fallback queue.

### Cuidados

- Chame `ParameterManager::beginBlock(...)` antes de adaptar automação; caso contrário, eventos podem ser limpos depois.
- Não chame `adaptProcessData` para `outputParameterChanges`; automação recebida do host vem de `inputParameterChanges`.
- Parâmetros sem `ParameterFlag::Automatable` serão rejeitados pelo Manager como `NotAutomatable`.
- O adapter não deve ser usado para presets, MIDI, note expression, modulação ou vozes.

---

## Fluxo VST3 recomendado com os adapters

```text
Steinberg::Vst::ProcessData
    ↓
VST3ProcessContextAdapter
    ↓
cvdsp::ProcessContext<T>
    ↓
ParameterManager::beginBlock(context)
    ↓
VST3AudioBufferAdapter
    ↓
AudioBufferView / ConstAudioBufferView
    ↓
DSP CV_DSP
```

Fluxo de bloco recomendado:

```text
1. Adaptar ProcessContext.
2. Chamar ParameterManager::beginBlock(context).
3. Adaptar automação VST3 com VST3ParameterAdapter.
4. Criar AudioBufferView/ConstAudioBufferView.
5. Processar áudio chamando ParameterManager::processSample() quando necessário.
6. Atualizar silenceFlags de output.
```

## Boas práticas específicas para VST3

- Não usar `ProcessData::numInputs` ou `numOutputs` como número de canais; eles indicam quantidade de buses.
- Usar `AudioBusBuffers::numChannels` para canais reais de um bus.
- Escolher `makeMutable32`/`makeConst32` somente para processamento 32-bit.
- Escolher `makeMutable64`/`makeConst64` somente para processamento 64-bit.
- Não reinterpretar `float` como `double` nem `double` como `float`.
- Não copiar áudio para buffers intermediários.
- Não guardar ponteiros VST3 fora do bloco atual.
- Tratar `numSamples == 0` como bloco sem áudio útil, não como falha fatal.
- Usar `silenceFlags` como otimização, não como fonte única de verdade.

## Relação atualizada entre Manager e Adapters/VST3

```text
Core
├── AudioBufferView<T>
└── ProcessContext<T>

Manager
├── ParameterDescriptor<T>
├── ParameterState<T>
└── ParameterManager<T>

Adapters/VST3
├── VST3ProcessContextAdapter
├── VST3AudioBufferAdapter
└── VST3ParameterAdapter

Fluxo
VST3 SDK → Adapters/VST3 → Core/Manager neutros → DSP modules
```

Essa arquitetura mantém o SDK Steinberg fora de `Core` e `Manager`, preserva compatibilidade futura com CLAP/JUCE/iPlug2 e evita duplicação das abstrações centrais da CV_DSP.
