# CV Vintage Hard Distortion VST3

Fase J do plano de pedais: segundo plug-in `.vst3` concreto usando o DSP `cvdsp::guitar::pedals::VintageHardDistortionDSP<float>`.

## Controles expostos

O controller registra automaticamente os 19 descritores do pedal: Bypass, Input, Distortion, Tone, Level, Mix, Oversampling, Quality, Pre HPF, Positive Threshold, Negative Threshold, Threshold Link, Asymmetry, Scoop Amount, Scoop Frequency, Scoop Q, High Bite, Fizz Cut e Post Low-Pass.

## Build manual

```bash
cmake -S examples/pedais/vintage_hard_distortion_vst3 -B /tmp/vintage_hard_distortion_vst3_build
cmake --build /tmp/vintage_hard_distortion_vst3_build --target vintage_hard_distortion_vst3
```

Em plataformas sem backend CV_GUI ativo, o `createView()` retorna `nullptr` e a DAW deve abrir o editor genérico/nativo de parâmetros.
