# expander_vst3

Plugin VST3 individual para **CV Expander** usando `CV_DSP/Dynamics/Expander.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> VST3ParameterAdapter
  -> ParameterManager
  -> cvdsp::dynamics::Expander<float>
  -> saída de áudio
```

## Parâmetros

- `Threshold`: -80 dB a 0 dB
- `Ratio`: 1:1 a 20:1
- `Attack`: 0.1 ms a 200 ms
- `Release`: 1 ms a 2000 ms

## Build MinGW64

```bat
cd examples\expander_vst3
cmake -S . -B build -G "MinGW Makefiles" -DEXPANDER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O CMake configura o output Windows em:

```text
build\out\expander_vst3\expander_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\expander_vst3`.
2. Faça rescan.
3. Carregue **CV Expander**.
4. Se o backend CV_GUI falhar, o controller retorna `nullptr` e a DAW usa o Generic Editor.
