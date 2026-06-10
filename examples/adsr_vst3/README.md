# adsr_vst3

Plugin VST3 individual para **CV ADSR** usando `CV_DSP/Modulation/ADSR.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::modulation::ADSR<float>
  -> saída de áudio
```

## Parâmetros

- `Gate`: 0 ou 1, dispara `noteOff()` / `noteOn()`
- `Attack`: 0.1 ms a 5000 ms
- `Decay`: 0.1 ms a 5000 ms
- `Sustain`: 0.0 a 1.0
- `Release`: 0.1 ms a 10000 ms

## Build

```bat
cd examples\adsr_vst3
cmake -S . -B build -G "MinGW Makefiles" -DADSR_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin: `build\out\adsr_vst3\adsr_vst3.vst3`.
