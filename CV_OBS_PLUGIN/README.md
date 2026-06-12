# CV_OBS_PLUGIN

`CV_OBS_PLUGIN` contém o módulo nativo do OBS que adicionará suporte VST3 ao fluxo de filtros de áudio do OBS usando a base `AUDIO_DSP`.

O produto principal deste diretório é uma DLL/plugin nativo do OBS:

```text
obs-audio-dsp-vst3.dll
```

> Importante: a meta deste projeto **não** é criar `obs-audio-dsp-vst3.vst3`. O alvo é uma DLL do OBS que aparece na janela de filtros do OBS e hospeda plugins `.vst3` escolhidos pelo usuário.

## Objetivo do produto

O plugin deve aparecer na janela de filtros de áudio do OBS como um novo filtro, por exemplo:

```text
AUDIO_DSP VST3
```

A experiência final esperada é:

1. O usuário instala `obs-audio-dsp-vst3.dll` no OBS.
2. O usuário coloca plugins `.vst3` em uma pasta `obs-vst3` ou escolhe pastas VST3 do sistema.
3. O filtro `AUDIO_DSP VST3` aparece na lista de filtros de áudio do OBS.
4. O usuário seleciona um VST3 para processar microfone, desktop audio ou outra fonte de áudio.
5. O usuário ajusta parâmetros pela interface do OBS e, quando suportado, pela janela nativa do plugin VST3.

## Escopo inicial

O primeiro alvo técnico é registrar um filtro de áudio nativo no OBS com:

- nome público: `AUDIO_DSP VST3`;
- identificador interno: `obs_audio_dsp_vst3_filter`;
- binário Windows: `obs-audio-dsp-vst3.dll`;
- tipo OBS: filtro de áudio (`OBS_SOURCE_TYPE_FILTER` + `OBS_SOURCE_AUDIO`).

Nesta etapa inicial, o filtro ainda é apenas a base do módulo. O processamento de áudio, o host VST3, o scanner de plugins e a interface avançada entram em etapas posteriores.

## Estrutura da pasta `obs-vst3`

A pasta `obs-vst3` será a área própria do plugin para dados relacionados a VST3:

```text
obs-vst3/
├── plugins/
├── presets/
├── chains/
├── cache.json
└── blacklist.json
```

Uso pretendido:

- `plugins/`: local opcional onde o usuário pode colocar plugins `.vst3` para o OBS carregar.
- `cache.json`: cache de plugins encontrados pelo scanner.
- `blacklist.json`: lista de plugins que falharam no scan/carregamento.
- `presets/`: presets do filtro e dos mapeamentos de parâmetros.
- `chains/`: cadeias combinando AUDIO_DSP e VST3.

## Código-fonte do OBS

Este projeto é um plugin externo. Ele não exige alterar o source code do OBS.

O módulo é compilado contra headers e bibliotecas do OBS/libobs compatíveis com a versão alvo. No Windows, o resultado esperado é uma DLL carregada pelo OBS como plugin nativo.

## Build inicial

O CMake deste diretório espera encontrar `obs-module.h` e a biblioteca `obs`/`libobs`. Informe esses caminhos quando estiver usando uma árvore/SDK do OBS local:

```bash
cmake -S CV_OBS_PLUGIN -B /tmp/cv_obs_plugin_build \
  -DLIBOBS_INCLUDE_DIR=/path/to/obs/libobs \
  -DLIBOBS_LIBRARY=/path/to/libobs
cmake --build /tmp/cv_obs_plugin_build
```

No Windows, use caminhos equivalentes do build/SDK do OBS 27.x e um gerador do Visual Studio compatível.

## Status

Status atual: scaffold inicial do módulo OBS nativo.

Próximas entregas planejadas no desenvolvimento:

- filtro pass-through seguro;
- controles iniciais na janela de filtros;
- processamento direto com `CV_DSP`;
- pasta e scanner `obs-vst3`;
- host VST3 mínimo sem GUI;
- mapeamento de parâmetros VST3;
- janela nativa VST3 quando viável.
