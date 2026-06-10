# Power Amp VST3

Exemplo VST3 de simulação de amplificador usando `CV_DSP/Guitar/PowerAmp.hpp` e a classe real `cvdsp::PowerAmp<float>`.

## Parâmetros expostos

- `Input Gain`: `0.0f .. 4.0f`.
- `Model`: `0.0f .. 3.0f`.
- `Output Gain`: `0.0f .. 4.0f`.

## GUI

O projeto usa `CV_GUI` quando o backend Win32/OpenGL3 estiver disponível. O editor apresenta os parâmetros VST3 em sliders Dear ImGui e permite observar/ajustar o ganho de estágio e a resposta tonal/frequencial exposta pelo DSP. Se a GUI não puder ser criada, o controller retorna `nullptr` e a DAW usa o Generic Editor.

## Build MinGW64

```bash
cmake -S examples/power_amp_vst3 -B build/power_amp_vst3 -G "MinGW Makefiles"
cmake --build build/power_amp_vst3 --parallel
```

Artefato Windows esperado:

```text
build/power_amp_vst3/out/power_amp_vst3/power_amp_vst3.vst3
```
