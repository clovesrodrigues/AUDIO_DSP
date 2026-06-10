# CV Classic Overdrive VST3

Fase I do plano de pedais: primeiro plug-in `.vst3` concreto usando o DSP `cvdsp::guitar::pedals::ClassicOverdriveDSP<float>`.

## Controles expostos

O controller registra automaticamente os 14 descritores do pedal: Bypass, Input, Drive, Tone, Level, Mix, Oversampling, Quality, Pre HPF, Mid Hump Gain, Mid Hump Frequency, Softness, Bias e Asymmetry.

## Build manual

```bash
cmake -S examples/pedais/classic_overdrive_vst3 -B /tmp/classic_overdrive_vst3_build
cmake --build /tmp/classic_overdrive_vst3_build --target classic_overdrive_vst3
```

Em plataformas sem backend CV_GUI ativo, o `createView()` retorna `nullptr` e a DAW deve abrir o editor genérico/nativo de parâmetros.
