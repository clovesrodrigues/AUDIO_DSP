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

1. ParameterDescriptor

Não guarda valor.

Não guarda automação.

Não guarda estado.

Ele apenas descreve o parâmetro.

Exemplo:

Cutoff

Descriptor:

ID = 1001

Nome = "Cutoff"

Unidade = "Hz"

Min = 20

Max = 20000

Default = 1000

Escala = Logarithmic

Ele é imutável.

Você cria uma vez.

Analogia:

Ficha técnica do parâmetro.
2. ParameterState

Agora entra o estado vivo.

Por exemplo:

Cutoff

Descriptor diz:

20Hz -> 20000Hz

Mas o valor atual muda.

Current = 1000Hz

Target = 5000Hz

Normalized = 0.74

Talvez também:

LinearSmoother

ou

ExponentialSmoother

Analogia:

É o coração batendo.

O descriptor é a identidade.

O state é o estado atual.

3. ParameterManager

Esse é o cara que o Codex chama de:

orquestrador

Porque ele gerencia todos os parâmetros.

Imagine um plugin com:

Gain
Mix
Threshold
Attack
Release
Cutoff
Resonance
Drive

Você terá:

8 ParameterDescriptor

e

8 ParameterState

O Manager controla tudo.

Visualmente:

ParameterManager

├── Gain
│   ├── Descriptor
│   └── State
│
├── Mix
│   ├── Descriptor
│   └── State
│
├── Threshold
│   ├── Descriptor
│   └── State
│
└── ...
O que ele faz na prática?
Registro

Na inicialização:

manager.registerParameter(
    cutoffDescriptor
);

manager.registerParameter(
    resonanceDescriptor
);
Lookup

Quando o DSP precisa:

cutoff = manager.getValue(kCutoff);
Automação

VST3 envia:

Cutoff = 0.82
sampleOffset = 123

Manager recebe:

manager.enqueueAutomation(
    cutoff,
    0.82,
    123
);
Sample Accurate

Chega no sample:

123

Manager faz:

cutoffState.setTarget(...)

e começa a rampa.

Snapshot

Quando salvar preset:

manager.createSnapshot();

Resultado:

Cutoff = 5342 Hz
Resonance = 0.72
Drive = 0.34

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
