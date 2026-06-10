# LFO VST3

Exemplo VST3 usando `CV_DSP/Modulation/LFO.hpp` e `cvdsp::modulation::LFO<float>`.

## Parâmetros expostos

- `Waveform`: Sine, Triangle, Saw ou Square.
- `Frequency`: frequência do LFO em Hz (`0.01 .. 50`).

## GUI

O projeto usa `CV_GUI` quando o backend Win32/OpenGL3 estiver disponível. Caso a view não seja criada, a DAW usa o Generic Editor.

## Build MinGW64

```bash
cmake -S examples/lfo_vst3 -B build/lfo_vst3 -G "MinGW Makefiles"
cmake --build build/lfo_vst3 --parallel
```
