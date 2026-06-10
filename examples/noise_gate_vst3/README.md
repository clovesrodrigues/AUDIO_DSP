# noise_gate_vst3

Plugin VST3 individual para **CV Noise Gate** usando `CV_DSP/Dynamics/NoiseGate.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> VST3ParameterAdapter
  -> ParameterManager
  -> cvdsp::dynamics::NoiseGate<float>
  -> saída de áudio
```

## Parâmetros

- `Threshold Open`: -80 dBFS a 0 dBFS
- `Threshold Close`: -90 dBFS a 0 dBFS
- `Attack`: 0.1 ms a 100 ms
- `Hold`: 0 ms a 500 ms
- `Release`: 1 ms a 1000 ms

## Build

```bat
cd examples\noise_gate_vst3
cmake -S . -B build -G "MinGW Makefiles" -DNOISE_GATE_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\noise_gate_vst3\noise_gate_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\noise_gate_vst3`.
2. Faça rescan.
3. Abra o plugin **CV Noise Gate**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
