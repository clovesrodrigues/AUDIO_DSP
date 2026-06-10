# Tarefa: Família de Pedais de Distorção para Guitarra

Este documento cataloga o plano de execução incremental para criar uma família de pedais digitais de distorção dentro da CV_DSP sem condensar trabalho demais em uma única etapa.

## Estado atual

- **Fase atual:** Semana/Fase G — Integração final.
- **Objetivo da fase:** consolidar a família com header agregador, exemplo unificado, documentação técnica e revisão final de safety/API.
- **Importante:** esta fase fecha o ciclo inicial para os quatro pedais e prepara o próximo trabalho de adapters/GUI/testes formais.

## Semana/Fase A — Planejamento e infraestrutura mínima

### Prompt A1 — Auditoria

Componentes existentes úteis para a família de pedais:

- `CV_DSP/Filters/Biquad.hpp`: base para HPF, LPF, peaking EQ, notch e shelves de pre/post EQ.
- `CV_DSP/Filters/DCBlocker.hpp`: remoção de offset DC após clipping assimétrico, bias e estágios multi-clipping.
- `CV_DSP/Math/Oversampling.hpp`: base inicial para caminhos Off/2x/4x/8x em estágios não-lineares.
- `CV_DSP/Math/FastMath.hpp`: aproximações rápidas de `tanh`, `atan`, soft clipping e conversões úteis para hot paths.
- `CV_DSP/Saturation/Waveshaper.hpp`: catálogo de curvas estáticas que pode ser reaproveitado por um clipper parametrizável.
- `CV_DSP/Core/ParameterSmoother.hpp`: suavização de ganho, mix e automações críticas para evitar zipper noise.
- `CV_DSP/Core/AudioBufferView.hpp`: processamento de blocos sem ownership e sem alocação.
- `CV_DSP/Manager/ParameterDescriptor.hpp`: metadados neutros para expor parâmetros futuramente a VST3/JUCE/CLAP/iPlug2.
- `CV_DSP/Guitar/AmpSimulator.hpp`: referência de composição modular para cadeias de guitarra.

Limitações e riscos técnicos identificados:

1. O oversampling atual é leve e adequado como ponto de partida, mas high-gain extremo poderá exigir filtros mais seletivos em fase futura.
2. O `Waveshaper` existente possui modos fixos; pedais exigem threshold, bias, assimetria, blend e drive parametrizáveis.
3. Frequências, Q, thresholds e ganhos extremos precisam de clamps para evitar NaN/Inf.
4. Setters podem recalcular coeficientes; `processSample`/`processBlock` devem permanecer sem alocação, locks e I/O.
5. Parâmetros audíveis precisam de suavização ou atualização controlada.

### Prompt A2 — Arquitetura sem código

Estrutura proposta:

```text
CV_DSP/Guitar/Pedals/
├── ROADMAP.md
├── PedalTypes.hpp
├── PedalParameterIDs.hpp
├── PedalParameterUtils.hpp
├── PedalGainStage.hpp
└── PedalMix.hpp
```

Fluxo DSP alvo para fases futuras:

```text
input
→ input gain
→ pre-filter / voice
→ oversampling opcional
→ clipper parametrizável
→ DC blocker
→ downsampling
→ post-filter / tone
→ output gain
→ dry/wet mix
```

Ordem planejada completa:

1. Fase A: infraestrutura mínima.
2. Fase B: core de distorção (`PedalClipper`, `PedalPreFilter`, `PedalPostFilter`, `PedalDriveCore`).
3. Fase C: `ClassicOverdriveDSP`.
4. Fase D: `VintageHardDistortionDSP`.
5. Fase E: `VintageFuzzDSP`.
6. Fase F: `ChainsawMetalDSP`.
7. Fase G: header agregador, exemplo, documentação e revisão final.

## Checklist da Fase A

- [x] Registrar auditoria e arquitetura.
- [x] Criar `PedalTypes.hpp`.
- [x] Criar `PedalParameterIDs.hpp`.
- [x] Criar `PedalParameterUtils.hpp`.
- [x] Criar `PedalGainStage.hpp`.
- [x] Criar `PedalMix.hpp`.
- [x] Verificar compilação por inclusão independente.

## Semana/Fase B — Core de distorção

### Prompt B1 — `PedalClipper.hpp`

Criado um waveshaper parametrizável com drive suavizado, bias suavizado, threshold positivo/negativo, link de threshold, knee, assimetria, blend e modos Hard/Soft/Tanh/Arctan/Cubic/Foldback/Hybrid.

### Prompt B2 — `PedalPreFilter.hpp`

Criado bloco de voice antes da distorção com HPF, LPF, shelves, low-mid, mid e presence usando biquads RBJ existentes.

### Prompt B3 — `PedalPostFilter.hpp`

Criado bloco de tone depois da distorção com macro Tone, HPF, LPF, bass/mid/treble/presence, fizz cut e notch.

### Prompt B4 — `PedalDriveCore.hpp`

Criado core comum com cadeia input gain → pre-filter → oversampling opcional → clipper → DC blocker → downsampling → post-filter → output gain → dry/wet.

## Checklist da Fase B

- [x] Criar `PedalClipper.hpp`.
- [x] Criar `PedalPreFilter.hpp`.
- [x] Criar `PedalPostFilter.hpp`.
- [x] Criar `PedalDriveCore.hpp`.
- [x] Verificar compilação por inclusão independente.

## Semana/Fase C — Primeiro pedal: Classic Overdrive

### Prompt C1 — DSP básico em `ClassicOverdriveDSP.hpp`

Criado o primeiro pedal concreto usando `PedalDriveCore`, com topologia mid-forward: HPF forte antes da saturação, clipping cúbico/soft, DC blocker pelo core e tone pós-distorção.

### Prompt C2 — Descritores de parâmetros

Adicionados descritores estáticos para Bypass, Input Gain, Drive, Tone, Level, Mix, Oversampling, Quality, Pre HPF, Mid Hump Gain/Frequency, Softness, Bias e Asymmetry.

### Prompt C3 — Teste/exemplo mínimo

Criado exemplo standalone em `examples/classic_overdrive_dsp` para compilar, configurar o pedal, processar buffer, verificar NaN/Inf e validar a tabela de parâmetros.

## Checklist da Fase C

- [x] Criar `ClassicOverdriveDSP.hpp`.
- [x] Adicionar descritores estáticos de parâmetros.
- [x] Criar smoke example mínimo.
- [x] Verificar compilação/processamento do exemplo.

## Semana/Fase D — Segundo pedal: Vintage Hard Distortion

### Prompt D1 — DSP básico em `VintageHardDistortionDSP.hpp`

Criado o segundo pedal concreto usando `PedalDriveCore`, com HPF mais baixo que overdrive, hard clipping, thresholds configuráveis e tone pós-distorção em estilo scooped.

### Prompt D2 — Descritores de parâmetros

Adicionados descritores estáticos para Bypass, Input Gain, Distortion, Tone, Level, Mix, Oversampling, Quality, Pre HPF, thresholds positivo/negativo, Threshold Link, Asymmetry, Scoop Amount/Frequency/Q, High Bite, Fizz Cut e Post LPF.

### Prompt D3 — Teste de estabilidade e oversampling

Criado exemplo standalone em `examples/vintage_hard_distortion_dsp` para compilar, configurar o pedal, processar buffer e silêncio, testar oversampling Off/2x/4x/8x e validar descritores.

## Checklist da Fase D

- [x] Criar `VintageHardDistortionDSP.hpp`.
- [x] Adicionar descritores estáticos de parâmetros.
- [x] Criar smoke example com oversampling Off/2x/4x/8x.
- [x] Verificar compilação/processamento do exemplo.

## Semana/Fase E — Terceiro pedal: Vintage Fuzz

### Prompt E1 — DSP básico em `VintageFuzzDSP.hpp`

Criado o terceiro pedal concreto usando `PedalDriveCore`, com topologia de fuzz assimétrico, oversampling 8x por padrão, clipping foldback e pós-filtragem agressiva.

### Prompt E2 — Cleanup/input load/starve

Adicionados controles de cleanup, input load, starve, low bloom e rectification, aproximando comportamento de fuzz vintage sem modelagem circuital pesada.

### Prompt E3 — Descritores de parâmetros

Adicionados descritores estáticos para Bypass, Input Gain, Fuzz, Level, Tone, Mix, Oversampling, Quality, Bias, Starve, Cleanup, Input Load, Asymmetry, Positive/Negative Gain, Foldback, Rectify, Pre HPF, Post LPF, Low Bloom e Gate.

### Prompt E4 — Teste de DC/silêncio/extremos

Criado exemplo standalone em `examples/vintage_fuzz_dsp` para compilar, processar casos extremos, testar rectification/gate e validar ausência de NaN/Inf ou DC runaway.

## Checklist da Fase E

- [x] Criar `VintageFuzzDSP.hpp`.
- [x] Adicionar cleanup/input load/starve.
- [x] Adicionar descritores estáticos de parâmetros.
- [x] Criar smoke example de DC/silêncio/extremos.
- [x] Verificar compilação/processamento do exemplo.

## Semana/Fase F — Quarto pedal: Chainsaw Metal

### Prompt F1 — DSP básico em `ChainsawMetalDSP.hpp`

Criado o quarto pedal concreto com gate opcional, input gain, low cut, pre-boost, dois estágios de clipping, DC blocker e pós-EQ ressonante low-mid/high-mid.

### Prompt F2 — Modos de voicing

Adicionados modos ClassicSwedish, ModernTight, DoomLoose e DeathMetalScoop para aplicar presets editáveis de low cut, fizz cut, hard threshold e boosts de médios.

### Prompt F3 — Descritores de parâmetros

Adicionados descritores estáticos para Gain, Voice Mode, Pre Boost, Stage 1/2, Hard Threshold, Interstage Gain, Low-Mid, High-Mid, Fizz Cut, Tight Low Cut e Gate.

### Prompt F4 — Teste extremo

Criado exemplo standalone em `examples/chainsaw_metal_dsp` para compilar, testar todos os voicings com oversampling Off/2x/4x/8x, boosts extremos, thresholds e validade dos descritores.

## Checklist da Fase F

- [x] Criar `ChainsawMetalDSP.hpp`.
- [x] Adicionar modos de voicing.
- [x] Adicionar descritores estáticos de parâmetros.
- [x] Criar smoke example extremo.
- [x] Verificar compilação/processamento do exemplo.

## Semana/Fase G — Integração final

### Prompt G1 — Header agregador `CV_DSP/Guitar/Pedals.hpp`

Criado header agregador para expor infraestrutura comum e os quatro pedais concretos por um único include.

### Prompt G2 — Exemplo unificado de pedalboard simples

Criado exemplo standalone em `examples/guitar_pedalboard_dsp` para preparar todos os pedais, processar buffers independentes e validar descritores.

### Prompt G3 — Documentação técnica dos pedais

Criado `CV_DSP/Guitar/Pedals/README.md` com arquitetura, headers compartilhados, descrição dos pedais, parâmetros, notas de real-time safety e checklist para novos pedais.

### Prompt G4 — Revisão de real-time safety

Revisados os headers da família para evitar alocação dinâmica, locks e I/O no hot path; smoke examples continuam fora do núcleo DSP.

### Prompt G5 — Revisão de API

Revisados includes, namespace, nomes públicos, descriptors e compilação por include agregado.

## Checklist da Fase G

- [x] Criar `CV_DSP/Guitar/Pedals.hpp`.
- [x] Criar exemplo unificado de pedalboard simples.
- [x] Criar documentação técnica dos pedais.
- [x] Executar revisão de real-time safety/API.
- [x] Verificar compilação dos smoke examples.

## Próxima fase sugerida

Com a família inicial completa, os próximos passos recomendados são:

1. Adicionar testes unitários formais para os blocos DSP.
2. Melhorar filtros de oversampling para modos high-gain extremos.
3. Criar adapters/GUI/VST3 para expor os parâmetros dos pedais.
4. Fazer medições espectrais/aliasing com FFT e sweeps controlados.


## Semana/Fase H — Expansão para pedais de dinâmica

### Prompt H1 — SustainerDSP

Planejado e implementado um pedal `SustainerDSP` independente do `PedalDriveCore`, com topologia de dinâmica: ganho de entrada, HPF de sidechain, `EnvelopeFollower`, gain computer de sustain com boost máximo limitado, `NoiseGate` com hysteresis/hold, suavização de ganho, nível de saída e dry/wet mix.

### Prompt H2 — Exemplo e documentação

Adicionado smoke example standalone em `examples/sustainer_dsp`, inclusão no agregador `CV_DSP/Guitar/Pedals.hpp`, documentação no README dos pedais e IDs neutros para parâmetros de sustain/dinâmica.

## Checklist da Fase H

- [x] Criar `SustainerDSP.hpp`.
- [x] Adicionar IDs de parâmetros para sustain, attack, release, ratio, makeup, max boost, detector mode e sidechain HPF.
- [x] Adicionar descritores estáticos.
- [x] Criar smoke example de nota decrescente, silêncio, ruído baixo e descritores.
- [x] Verificar compilação/processamento do exemplo.


## Semana/Fase I — Primeiro pedal de filtro expressivo

### Prompt I1 — WahWahDSP manual

Criado `WahWahDSP` como pedal independente do `PedalDriveCore`, usando `StateVariableFilter` em modo band-pass, suavização one-pole de expressão, taper musical, mapeamento logarítmico de frequência, Q dinâmico, ganho de band-pass, corpo seco, drive vintage leve opcional, output level e dry/wet mix.

### Prompt I2 — Exemplo e integração

Adicionado smoke example standalone em `examples/wah_wah_dsp`, inclusão no agregador `CV_DSP/Guitar/Pedals.hpp`, documentação no README dos pedais, integração no smoke unificado de pedalboard e IDs neutros para expressão/filtro wah.

## Checklist da Fase I

- [x] Criar `WahWahDSP.hpp`.
- [x] Adicionar IDs de parâmetros para expression, expression source, min/max frequency, min/max Q, taper, filter drive, band-pass gain e dry gain.
- [x] Adicionar descritores estáticos.
- [x] Criar smoke example com sweep de expressão, silêncio e descritores.
- [x] Verificar compilação/processamento do exemplo.


## Semana/Fase J — Expression Engine determinístico

### Prompt J1 — `cvdsp::control::ExpressionEngine`

Criada a primeira versão do `ExpressionEngine` em `CV_DSP/Control`, fora da família de pedais, como gerador real-time-safe de controle `p` em `[0, 1]`. A análise usa envelope, energia rápida/lenta, detecção de transientes, densidade rítmica e BPM/PPQ do host para alternar entre estados `Idle`, `AttackAccent`, `Rhythmic` e `Vocal`, sem pitch tracking ou inferência neural nesta fase.

### Prompt J2 — Integração com WahWahDSP

Adicionado smoke example standalone em `examples/expression_engine_dsp` demonstrando o `ExpressionEngine` alimentando `WahWahDSP` por blocos, com validação de saída finita e expressão normalizada.

## Checklist da Fase J

- [x] Criar `CV_DSP/Control/ExpressionEngine.hpp`.
- [x] Manter o motor independente de VST3/JUCE/CLAP e sem alocações no hot path.
- [x] Implementar análise de envelope, transientes, densidade e grid rítmico via BPM/PPQ.
- [x] Gerar estados determinísticos `Idle`, `AttackAccent`, `Rhythmic` e `Vocal`.
- [x] Criar smoke example integrado com `WahWahDSP`.
- [x] Verificar compilação/processamento do exemplo.


## Semana/Fase K — Phaser clássico

### Prompt K1 — Phaser genérico e wrapper de pedal

Criado `CV_DSP/Effects/Phaser.hpp` como efeito genérico mono baseado em cascata de filtros all-pass de primeira ordem, varredura logarítmica por LFO, feedback limitado, estágios pares configuráveis e soma wet/dry 50/50 para cancelamentos móveis. Criado também `PhaserDSP` em `CV_DSP/Guitar/Pedals` para expor defaults e descritores de pedal.

### Prompt K2 — Exemplo e integração

Adicionado smoke example standalone em `examples/phaser_dsp`, inclusão no agregador `CV_DSP/Guitar/Pedals.hpp`, documentação no README dos pedais, integração no smoke unificado de pedalboard e IDs neutros para rate/depth/feedback/waveform de modulação.

## Checklist da Fase K

- [x] Criar `CV_DSP/Effects/Phaser.hpp`.
- [x] Criar `CV_DSP/Guitar/Pedals/PhaserDSP.hpp`.
- [x] Adicionar IDs de parâmetros para modulation rate/depth, feedback e LFO waveform.
- [x] Adicionar descritores estáticos ao wrapper de pedal.
- [x] Criar smoke example do Phaser genérico e do wrapper `PhaserDSP`.
- [x] Verificar compilação/processamento do exemplo.
