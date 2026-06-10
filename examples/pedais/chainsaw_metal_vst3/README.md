# CV Chainsaw Metal VST3

Fase L do plano de pedais: quarto plug-in `.vst3` concreto usando o DSP `cvdsp::guitar::pedals::ChainsawMetalDSP<float>`.

## Controles expostos

O controller registra automaticamente os 27 descritores do pedal: Bypass, Input, Gain, Level, Mix, Oversampling, Quality, Voice Mode, Pre Boost Gain/Frequency/Q, Stage 1 Drive/Softness, Stage 2 Drive, Hard Threshold, Interstage Gain, Low-Mid Gain/Frequency/Q, High-Mid Gain/Frequency/Q, Fizz Cut, Tight Low Cut, Gate Enable, Gate Threshold e Gate Release.

## Build manual

```bash
cmake -S examples/pedais/chainsaw_metal_vst3 -B /tmp/chainsaw_metal_vst3_build
cmake --build /tmp/chainsaw_metal_vst3_build --target chainsaw_metal_vst3
```

Em plataformas sem backend CV_GUI ativo, o `createView()` retorna `nullptr` e a DAW deve abrir o editor genérico/nativo de parâmetros.
