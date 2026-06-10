# state_variable_filter_vst3

Plugin VST3 individual para **State Variable Filter** usando `CV_DSP/Filters/StateVariableFilter.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::filters::StateVariableFilter<float>
  -> saída de áudio
```

## Parâmetros

- `Mode`: LowPass, HighPass, BandPass ou Notch
- `Cutoff`: 20 Hz a 20 kHz
- `Resonance`: 0.1 a 40 Q

## Build

```bat
cd examples\state_variable_filter_vst3
cmake -S . -B build -G "MinGW Makefiles" -DSTATE_VARIABLE_FILTER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\state_variable_filter_vst3\state_variable_filter_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\state_variable_filter_vst3`.
2. Faça rescan.
3. Abra o plugin **CV State Variable Filter**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
