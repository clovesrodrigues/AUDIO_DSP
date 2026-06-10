# Tube Preamp VST3

Exemplo VST3 de simulação de amplificador usando `CV_DSP/Guitar/TubePreamp.hpp` e a classe real `cvdsp::TubePreamp<float>`.

## Parâmetros expostos

- `Drive`: `0.0f .. 30.0f`.
- `Bias`: `-1.0f .. 1.0f`.
- `Plate Voltage`: `80.0f .. 400.0f`.
- `Stages`: `1.0f .. 4.0f`.
- `Output Gain`: `0.0f .. 4.0f`.

## GUI

O projeto usa `CV_GUI` quando o backend Win32/OpenGL3 estiver disponível. O editor apresenta os parâmetros VST3 em sliders Dear ImGui e permite observar/ajustar o ganho de estágio e a resposta tonal/frequencial exposta pelo DSP. Se a GUI não puder ser criada, o controller retorna `nullptr` e a DAW usa o Generic Editor.

## Build MinGW64

```bash
cmake -S examples/tube_preamp_vst3 -B build/tube_preamp_vst3 -G "MinGW Makefiles"
cmake --build build/tube_preamp_vst3 --parallel
```

Artefato Windows esperado:

```text
build/tube_preamp_vst3/out/tube_preamp_vst3/tube_preamp_vst3.vst3
```
