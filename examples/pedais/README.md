# Pedais VST3 — família completa

Esta pasta contém os exemplos `.vst3` de guitarra construídos a partir dos DSPs header-only em `CV_DSP/Guitar/Pedals` e dos adaptadores compartilhados em `examples/pedais/common`.

## Status das fases

| Fase | Status | Entrega |
| --- | --- | --- |
| H | Concluída | Foundation comum para descriptors e estado VST3. |
| I | Concluída | `classic_overdrive_vst3` com 14 controles. |
| J | Concluída | `vintage_hard_distortion_vst3` com 19 controles. |
| K | Concluída | `vintage_fuzz_vst3` com 22 controles. |
| L | Concluída | `chainsaw_metal_vst3` com 27 controles. |
| M | Concluída | Consolidação, build helper e roteiro de polimento visual. |
| N | Concluída | `sustainer_vst3`, `phaser_vst3` e `wah_wah_vst3` adicionados ao build helper. |

## Projetos VST3

| Plugin | Pasta | DSP | Controles | Observações |
| --- | --- | --- | --- | --- |
| CV Classic Overdrive | `classic_overdrive_vst3` | `ClassicOverdriveDSP<float>` | 14 | Overdrive clássico. |
| CV Vintage Hard Distortion | `vintage_hard_distortion_vst3` | `VintageHardDistortionDSP<float>` | 19 | Distorção hard clipping vintage. |
| CV Vintage Fuzz | `vintage_fuzz_vst3` | `VintageFuzzDSP<float>` | 22 | Fuzz vintage com estágios e voicing. |
| CV Chainsaw Metal | `chainsaw_metal_vst3` | `ChainsawMetalDSP<float>` | 27 | Metal high-gain com cadeia tonal ampla. |
| CV Sustainer | `sustainer_vst3` | `SustainerDSP<float>` | 15 | Compressor/sustainer com gate, detector e sidechain HPF. |
| CV Phaser | `phaser_vst3` | `PhaserDSP<float>` | 11 | Phaser com rate, depth, feedback, stages, sweep e LFO waveform. |
| CV Wah-Wah | `wah_wah_vst3` | `WahWahDSP<float>` | 14 | Wah manual/automação com chave `Expr Eng` para ExpressionEngine. |

## Foundation comum

- `common/PedalVST3ParameterAdapter.hpp`: converte `cvdsp::manager::ParameterDescriptor<T>` em metadados VST3 (`ParamID`, título, unidade, `stepCount`, flags e valor default normalizado) e registra esses parâmetros no `ParameterContainer` do controller.
- `common/PedalVST3ParameterState.hpp`: mantém um cache fixo de valores normalizados e aplica o último ponto de automação recebido por bloco, sem alocação dinâmica.
- Cada plug-in mantém seu próprio `CMakeLists.txt`, `source/`, `resource/` e identificadores VST3, mas segue a mesma arquitetura de processor/controller para facilitar manutenção.

## Tutorial — build de todos os pedais

### 1. Pré-requisitos

Execute os comandos a partir da raiz do repositório `AUDIO_DSP`. Você precisa de:

- `bash`;
- `cmake` 3.14 ou superior;
- um compilador C++20 (`g++`, `clang++` ou MSVC equivalente);
- o VST3 SDK bundled já presente em `backends/vst3sdk`.

### 2. Build padrão

```bash
examples/pedais/build_all_pedals.sh
```

Esse comando configura e compila todos os pedais listados no array interno do script:

```text
classic_overdrive_vst3
vintage_hard_distortion_vst3
vintage_fuzz_vst3
chainsaw_metal_vst3
sustainer_vst3
phaser_vst3
wah_wah_vst3
```

Por padrão, a pasta de build fica fora do repositório para não poluir a árvore de fontes:

```text
/tmp/cv_dsp_pedais_vst3_build
```

### 3. Escolher uma pasta de build customizada

```bash
examples/pedais/build_all_pedals.sh /tmp/minha_build_pedais
```

Use esse modo quando quiser manter builds separados por experimento, compilador ou branch.

### 4. Local dos artefatos em Linux

Em Linux, cada `.vst3` gerado fica dentro da pasta do respectivo plug-in:

```text
<build-root>/<plugin>/VST3/<plugin>.vst3/Contents/x86_64-linux/<plugin>.so
```

Exemplo para o Wah-Wah usando o build padrão:

```text
/tmp/cv_dsp_pedais_vst3_build/wah_wah_vst3/VST3/wah_wah_vst3.vst3/Contents/x86_64-linux/wah_wah_vst3.so
```

### 5. Instalação manual para teste em DAW

Copie a pasta `.vst3` inteira, não apenas o arquivo `.so`.

Exemplo em Linux:

```bash
mkdir -p ~/.vst3
cp -R /tmp/cv_dsp_pedais_vst3_build/wah_wah_vst3/VST3/wah_wah_vst3.vst3 ~/.vst3/
```

Repita para os demais pedais que quiser testar. Depois, peça para a DAW reescanear os plug-ins VST3.

### 6. Build individual

Para compilar apenas um pedal:

```bash
cmake -S examples/pedais/wah_wah_vst3 -B /tmp/wah_wah_vst3_build
cmake --build /tmp/wah_wah_vst3_build --target wah_wah_vst3
```

Troque a pasta e o target por qualquer um destes nomes:

```text
classic_overdrive_vst3
vintage_hard_distortion_vst3
vintage_fuzz_vst3
chainsaw_metal_vst3
sustainer_vst3
phaser_vst3
wah_wah_vst3
```

### 7. Warnings conhecidos

Em Linux, o VST3 SDK bundled pode emitir warnings de formato (`%lld`/`%llu`) em arquivos da Steinberg. Esses warnings vêm do SDK externo e não impedem a geração dos `.vst3`.


## O que não entra neste build helper

`build_all_pedals.sh` compila apenas pedais de guitarra em `examples/pedais/*_vst3`.
Utilitários que não são pedais, como `examples/spectral_noise_reducer_vst3`, devem
ser compilados separadamente para manter clara a arquitetura: o redutor espectral
fica no início da cadeia de gravação, antes do set de pedais.

Build do utilitário SpectralNoiseReducer:

```bash
cmake -S examples/spectral_noise_reducer_vst3 -B /tmp/spectral_noise_reducer_vst3_build
cmake --build /tmp/spectral_noise_reducer_vst3_build
```

## GUI / fallback

Os projetos tentam usar a CV_GUI quando o backend está disponível. Em plataformas sem backend ativo, `createView()` retorna `nullptr`, permitindo que a DAW abra o editor genérico/nativo de parâmetros. Isso mantém os plug-ins utilizáveis mesmo quando a GUI customizada não está pronta, não compila ou não está disponível em tempo real.

O roteiro visual detalhado está em `GUI_ROADMAP.md`.
