# Pedais VST3 — família completa

Esta pasta contém os quatro plug-ins `.vst3` de guitarra construídos a partir dos DSPs header-only em `CV_DSP/Guitar/Pedals`.

## Status das fases

| Fase | Status | Entrega |
| --- | --- | --- |
| H | Concluída | Foundation comum para descriptors e estado VST3. |
| I | Concluída | `classic_overdrive_vst3` com 14 controles. |
| J | Concluída | `vintage_hard_distortion_vst3` com 19 controles. |
| K | Concluída | `vintage_fuzz_vst3` com 22 controles. |
| L | Concluída | `chainsaw_metal_vst3` com 27 controles. |
| M | Concluída | Consolidação, build helper e roteiro de polimento visual. |

## Projetos VST3

| Plugin | Pasta | DSP | Controles |
| --- | --- | --- | --- |
| CV Classic Overdrive | `classic_overdrive_vst3` | `ClassicOverdriveDSP<float>` | 14 |
| CV Vintage Hard Distortion | `vintage_hard_distortion_vst3` | `VintageHardDistortionDSP<float>` | 19 |
| CV Vintage Fuzz | `vintage_fuzz_vst3` | `VintageFuzzDSP<float>` | 22 |
| CV Chainsaw Metal | `chainsaw_metal_vst3` | `ChainsawMetalDSP<float>` | 27 |

## Foundation comum

- `common/PedalVST3ParameterAdapter.hpp`: converte `cvdsp::manager::ParameterDescriptor<T>` em metadados VST3 (`ParamID`, título, unidade, `stepCount`, flags e valor default normalizado) e registra esses parâmetros no `ParameterContainer` do controller.
- `common/PedalVST3ParameterState.hpp`: mantém um cache fixo de valores normalizados e aplica o último ponto de automação recebido por bloco, sem alocação dinâmica.

## Build de todos os pedais

```bash
examples/pedais/build_all_pedals.sh
```

Para escolher outra pasta de build:

```bash
examples/pedais/build_all_pedals.sh /tmp/minha_build_pedais
```

Em Linux, os artefatos ficam em:

```text
<build-root>/<plugin>/VST3/<plugin>.vst3/Contents/x86_64-linux/
```

## Build individual

```bash
cmake -S examples/pedais/classic_overdrive_vst3 -B /tmp/classic_overdrive_vst3_build
cmake --build /tmp/classic_overdrive_vst3_build --target classic_overdrive_vst3
```

Troque a pasta e o target para `vintage_hard_distortion_vst3`, `vintage_fuzz_vst3` ou `chainsaw_metal_vst3`.

## GUI / fallback

Os projetos usam a CV_GUI quando o backend está disponível. Em plataformas sem backend ativo, `createView()` retorna `nullptr`, permitindo que a DAW abra o editor genérico/nativo de parâmetros.

O roteiro visual detalhado está em `GUI_ROADMAP.md`.
