# Oscillator VST3

Exemplo VST3 usando `CV_DSP/Modulation/Oscillator.hpp` e `cvdsp::modulation::Oscillator<float>`.

## Parâmetros expostos

- `Waveform`: Sine, Triangle, Saw ou Square.
- `Frequency`: frequência do oscilador em Hz (`20 .. 20000`).
- `Phase`: fase inicial em graus (`0 .. 360`).

## GUI

O projeto usa `CV_GUI` quando o backend Win32/OpenGL3 estiver disponível. Caso a view não seja criada, a DAW usa o Generic Editor.

## Build MinGW64

```bash
cmake -S examples/oscillator_vst3 -B build/oscillator_vst3 -G "MinGW Makefiles"
cmake --build build/oscillator_vst3 --parallel
```
