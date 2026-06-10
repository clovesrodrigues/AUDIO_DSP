# CV Vintage Fuzz VST3

Fase K do plano de pedais: terceiro plug-in `.vst3` concreto usando o DSP `cvdsp::guitar::pedals::VintageFuzzDSP<float>`.

## Controles expostos

O controller registra automaticamente os 22 descritores do pedal: Bypass, Input, Fuzz, Level, Tone, Mix, Oversampling, Quality, Bias, Starve, Cleanup, Input Load, Asymmetry, Positive Gain, Negative Gain, Foldback Amount, Rectify Mode, Pre HPF, Post LPF, Low Bloom, Gate Enable e Gate Threshold.

## Build manual

```bash
cmake -S examples/pedais/vintage_fuzz_vst3 -B /tmp/vintage_fuzz_vst3_build
cmake --build /tmp/vintage_fuzz_vst3_build --target vintage_fuzz_vst3
```

Em plataformas sem backend CV_GUI ativo, o `createView()` retorna `nullptr` e a DAW deve abrir o editor genérico/nativo de parâmetros.
