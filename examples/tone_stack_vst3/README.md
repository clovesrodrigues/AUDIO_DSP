# Tone Stack VST3

Exemplo VST3 de tone stack clássico usando `CV_DSP/Guitar/ToneStack.hpp` e a classe real `cvdsp::FenderToneStack<float>`.

## Parâmetros expostos

- `Bass`: controle normalizado `0.0 .. 1.0`.
- `Mid`: controle normalizado `0.0 .. 1.0`.
- `Treble`: controle normalizado `0.0 .. 1.0`.

## GUI

O projeto usa `CV_GUI` quando o backend Win32/OpenGL3 estiver disponível. O editor genérico do `CV_GUI` expõe Bass, Mid e Treble como sliders Dear ImGui; se a view não puder ser criada, a DAW usa o Generic Editor.

## Build MinGW64

```bash
cmake -S examples/tone_stack_vst3 -B build/tone_stack_vst3 -G "MinGW Makefiles"
cmake --build build/tone_stack_vst3 --parallel
```

Artefato Windows esperado:

```text
build/tone_stack_vst3/out/tone_stack_vst3/tone_stack_vst3.vst3
```
