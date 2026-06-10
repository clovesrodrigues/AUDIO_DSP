# Amp Simulator VST3

Exemplo VST3 de simulação de amplificador usando `CV_DSP/Guitar/AmpSimulator.hpp` e a classe real `cvdsp::AmpSimulator<float>`.

## Parâmetros expostos

- `Input Gain`: `0.0f .. 4.0f`.
- `Preamp Drive`: `0.0f .. 30.0f`.
- `Bass`: `0.0f .. 1.0f`.
- `Mid`: `0.0f .. 1.0f`.
- `Treble`: `0.0f .. 1.0f`.
- `Presence`: `0.0f .. 1.0f`.
- `Power Model`: `0.0f .. 3.0f`.
- `Output Gain`: `0.0f .. 4.0f`.

## GUI

O projeto usa `CV_GUI` quando o backend Win32/OpenGL3 estiver disponível. O editor apresenta os parâmetros VST3 em sliders Dear ImGui e permite observar/ajustar o ganho de estágio e a resposta tonal/frequencial exposta pelo DSP. Se a GUI não puder ser criada, o controller retorna `nullptr` e a DAW usa o Generic Editor.

## Build MinGW64

```bash
cmake -S examples/amp_simulator_vst3 -B build/amp_simulator_vst3 -G "MinGW Makefiles"
cmake --build build/amp_simulator_vst3 --parallel
```

Artefato Windows esperado:

```text
build/amp_simulator_vst3/out/amp_simulator_vst3/amp_simulator_vst3.vst3
```
