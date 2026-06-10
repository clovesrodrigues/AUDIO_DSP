# Stereo Width VST3

Exemplo VST3 usando `CV_DSP/Spatial/StereoWidth.hpp` e a classe `cvdsp::spatial::StereoWidth<float>`.

## Parâmetros expostos

- `Mid Gain`: ganho linear do componente Mid (`0.0 .. 2.0`).
- `Side Gain`: ganho linear do componente Side (`0.0 .. 2.0`).
- `Stereo Width`: largura estéreo aplicada em domínio Mid/Side (`0.0 .. 2.0`).

## Layout estéreo

O processador anuncia entrada e saída estéreo VST3. O processamento só aplica Mid/Side quando há pelo menos dois canais no bus; buses mono são repassados sem alteração e canais extras são copiados para preservar o layout do host.

## GUI

O projeto tenta habilitar `CV_GUI` automaticamente em Windows com `-DSTEREO_WIDTH_VST3_ENABLE_CV_GUI=ON`. Se o backend Win32/OpenGL3 não estiver disponível, o controller retorna `nullptr` e a DAW usa o editor genérico de parâmetros.

## Build MinGW64

```bash
cmake -S examples/stereo_width_vst3 -B build/stereo_width_vst3 -G "MinGW Makefiles"
cmake --build build/stereo_width_vst3 --parallel
```

Artefato Windows esperado:

```text
build/stereo_width_vst3/out/stereo_width_vst3/stereo_width_vst3.vst3
```
