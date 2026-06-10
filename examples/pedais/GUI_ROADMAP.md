# Pedais VST3 — Fase M: roteiro de polimento visual

Esta fase fecha o ciclo de criação dos quatro VST3s e deixa o próximo trabalho claramente separado: melhorar a experiência visual da CV_GUI sem mexer no DSP.

## Estado atual

Os quatro plug-ins usam a mesma estratégia:

1. O DSP declara `ParameterDescriptor<T>` com nome, unidade, faixa, default, grupo e flags.
2. O controller VST3 registra os descriptors por meio de `common/PedalVST3ParameterAdapter.hpp`.
3. O processor VST3 consome automação por `common/PedalVST3ParameterState.hpp` e aplica os valores ao DSP.
4. Quando a CV_GUI está disponível, `createView()` abre a janela Dear ImGui; caso contrário, retorna `nullptr` para a DAW abrir o editor genérico/nativo.

## Opção C — GUI híbrida recomendada

A próxima evolução deve manter o editor genérico funcional, mas organizar os controles em uma UI musical:

- **Header do pedal**: nome, bypass, medidor simples de entrada/saída e seletor de oversampling/quality.
- **Seção Global**: Input, Level, Mix.
- **Seção Drive/Clip**: parâmetros centrais de saturação e clipping.
- **Seção Voice/Pre**: filtros e controles antes da distorção.
- **Seção Tone/Post**: filtros, EQ e presença depois da distorção.
- **Seção Gate/Advanced**: noise gate, modos, thresholds e parâmetros de CPU.

## Tipos de controle sugeridos

- **Toggle/botão**: Bypass, Gate Enable, Threshold Link.
- **Combo**: Oversampling, Quality, Voice Mode, Rectify Mode.
- **Slider horizontal**: parâmetros técnicos de frequência, Q e threshold.
- **Knob futuro**: Drive/Fuzz/Gain, Tone, Level, Mix, Bias, Starve, Scoop e EQ gain.

## Ordem recomendada para implementar a GUI

1. Criar um modelo visual comum que lê `ParameterDescriptor<T>` e agrupa por `groupName`.
2. Fazer layout por seções usando ImGui, ainda com sliders padrão.
3. Adicionar toggles e combos com labels melhores para booleanos/enums.
4. Adicionar um tema escuro/metálico por pedal, sem bitmap pesado inicialmente.
5. Só depois criar knobs customizados, mantendo sliders como fallback.

## Critério de pronto

A GUI polida estará pronta quando cada pedal abrir com controles agrupados, nomes legíveis, editor genérico de fallback preservado e sem alocação/estado pesado no caminho de áudio.
