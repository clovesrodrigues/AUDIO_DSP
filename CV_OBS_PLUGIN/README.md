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

Status atual: Fase 2 fechada e Fase 3 iniciada com modelo de armazenamento `obs-vst3`, `cache.json` e `blacklist.json`.

Entregas concluídas:

- filtro pass-through seguro que devolve o áudio recebido sem alterar buffers, carregar VST3, abrir arquivos ou executar scanner;
- propriedade inicial `Bypass` na janela de filtros do OBS;
- default e atualização persistente do estado `Bypass` via configurações do OBS;
- documentação/checklist de build e teste para OBS 27 x64;
- gate da Fase 1 concluído sem scanner, host VST3, GUI VST3 ou MIDI;
- adaptador inicial OBS áudio → `CV_DSP`;
- ganho interno simples controlado por `Internal Gain (dB)`;
- caminho de áudio revisado para evitar I/O, scanner, VST3 e alocação dinâmica no callback;
- gate da Fase 2 concluído;
- modelo de pastas `obs-vst3` definido;
- estruturas iniciais para `cache.json` e `blacklist.json` definidas.

Próximas entregas planejadas no desenvolvimento:

- scanner VST3 sem host de áudio;
- host VST3 mínimo sem GUI;
- mapeamento de parâmetros VST3;
- janela nativa VST3 quando viável.

## Build OBS 27 x64 — checklist de desenvolvimento

Esta seção documenta o fluxo mínimo para compilar e validar o plugin contra OBS 27 x64.

Pré-requisitos esperados:

- OBS 27 x64 ou uma árvore/SDK compatível do OBS 27 x64;
- headers do libobs contendo `obs-module.h`;
- biblioteca de link do libobs compatível com o compilador usado;
- CMake 3.16 ou superior;
- toolchain Windows x64 compatível com o build do OBS alvo.

Configuração CMake esperada:

```bash
cmake -S CV_OBS_PLUGIN -B /tmp/cv_obs_plugin_build \
  -DLIBOBS_INCLUDE_DIR=/path/to/obs/libobs \
  -DLIBOBS_LIBRARY=/path/to/libobs
cmake --build /tmp/cv_obs_plugin_build
```

Checklist manual no OBS 27 x64:

1. Copiar `obs-audio-dsp-vst3.dll` para a pasta de plugins nativos do OBS usada no ambiente de teste.
2. Iniciar o OBS e confirmar que o módulo carrega sem erro fatal.
3. Adicionar o filtro `AUDIO_DSP VST3` em uma fonte de áudio.
4. Confirmar que a propriedade `Bypass` aparece na janela de filtros.
5. Alternar `Bypass`, fechar/reabrir propriedades e confirmar persistência no projeto OBS.
6. Monitorar o áudio e confirmar que ele permanece em pass-through, sem alteração intencional.
7. Remover o filtro e confirmar que o OBS não apresenta crash.

## Gate da Fase 1

A Fase 1 de infraestrutura OBS funcional está fechada.

Estado aprovado ao fechar a fase:

- o produto continua sendo uma DLL/plugin nativo do OBS, não um plugin VST3;
- o filtro `AUDIO_DSP VST3` está registrado como filtro de áudio;
- o callback `filter_audio` permanece em pass-through;
- a propriedade `Bypass` existe e é persistida pelo OBS;
- não há scanner VST3 ativo;
- não há host VST3 ativo;
- não há GUI nativa VST3;
- não há suporte MIDI/instrumentos ainda.

## Início da Fase 2 — adaptador OBS para CV_DSP

A Fase 2 começa com uma camada de adaptação entre o áudio recebido do OBS e a biblioteca `CV_DSP`.

Nesta etapa, o adaptador apenas:

- recebe `obs_audio_data`;
- valida se existem frames e canais;
- coleta ponteiros de canais sem copiar amostras;
- expõe uma visão `cvdsp::AudioBufferView<float>` para uso futuro;
- preserva o comportamento pass-through do filtro.

O adaptador ainda não aplica ganho, EQ, dinâmica, reverb, VST3 ou qualquer outro processamento.

## Fase 2 — DSP interno inicial

O primeiro processamento direto com `CV_DSP` é um ganho interno simples controlado por `Internal Gain (dB)`.

Comportamento atual:

- `Bypass` ligado: o filtro retorna o áudio sem aplicar ganho, preservando o comportamento pass-through;
- `Bypass` desligado: o filtro aplica o ganho interno aos buffers recebidos do OBS;
- `Internal Gain (dB)` aceita valores de -24 dB a +24 dB;
- o valor padrão é 0 dB;
- o valor é persistido pelas configurações do OBS.

## Segurança de tempo real do caminho de áudio

O caminho atual de `filter_audio` segue estas restrições:

- não carrega VST3;
- não executa scanner;
- não abre arquivos;
- não acessa rede;
- não cria cache ou blacklist;
- não aloca memória dinâmica dentro do callback de áudio;
- usa estado lido por `std::atomic` para `Bypass` e ganho linear;
- cria apenas objetos leves de stack para adaptar os buffers do OBS;
- processa diretamente os buffers existentes quando `Bypass` está desligado.

Operações potencialmente mais caras, como conversão de dB para ganho linear, são feitas no callback de atualização de configuração, não no loop de áudio.

## Gate da Fase 2

A Fase 2 de DSP interno direto está fechada para o escopo inicial.

Estado aprovado ao fechar a fase:

- o adaptador OBS áudio → `CV_DSP` existe e usa visão zero-copy;
- o ganho interno simples funciona como primeiro DSP direto;
- `Bypass` preserva o caminho pass-through quando ligado;
- `Internal Gain (dB)` é controlado pela UI do OBS e persistido pelo OBS;
- o callback de áudio não executa I/O, scanner, cache, blacklist ou carregamento VST3;
- o projeto ainda não instancia plugins `.vst3`.

## Fase 3 — modelo de armazenamento VST3

A Fase 3 começa definindo o modelo de armazenamento que será usado pelo scanner VST3 em uma etapa posterior.

Layout planejado:

```text
obs-vst3/
├── plugins/
├── presets/
├── chains/
├── cache.json
└── blacklist.json
```

Uso planejado:

- `plugins/`: local opcional para o usuário colocar plugins `.vst3` específicos deste plugin OBS;
- `presets/`: presets salvos para plugins e parâmetros;
- `chains/`: cadeias futuras combinando módulos internos e VST3;
- `cache.json`: índice de plugins encontrados pelo scanner;
- `blacklist.json`: registro de plugins que falharam ou devem ser ignorados.

Nesta etapa, o projeto apenas define nomes e estruturas de dados. Ele ainda não cria pastas em runtime, não abre arquivos, não faz varredura e não carrega VST3.

### Schema planejado de `cache.json`

Versão inicial: `1`.

Campos planejados por plugin:

- `path`: caminho do bundle/pasta `.vst3`;
- `name`: nome legível do plugin quando disponível;
- `vendor`: fabricante quando disponível;
- `version`: versão quando disponível;
- `classId`: identificador VST3 quando disponível;
- `category`: categoria VST3 quando disponível;
- `status`: estado do scan;
- `lastModifiedUnixSeconds`: timestamp do arquivo/bundle observado;
- `scannedAtUnixSeconds`: timestamp da última varredura.

### Schema planejado de `blacklist.json`

Versão inicial: `1`.

Campos planejados por entrada:

- `path`: caminho do plugin problemático;
- `classId`: identificador VST3 quando conhecido;
- `reason`: motivo resumido para ignorar o plugin;
- `lastError`: erro mais recente observado;
- `failedAtUnixSeconds`: timestamp da última falha;
- `failureCount`: número de falhas registradas.
