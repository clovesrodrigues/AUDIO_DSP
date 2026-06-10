# limiter_vst3

Plugin VST3 individual para **CV Limiter** usando `CV_DSP/Dynamics/Limiter.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> VST3ParameterAdapter
  -> ParameterManager
  -> cvdsp::dynamics::Limiter<float>
  -> saída de áudio
```

## Parâmetros

- `Threshold`: -24 dBFS a 0 dBFS
- `Release`: 1 ms a 1000 ms
- `Output Gain`: -12 dB a +12 dB

## Build MinGW64

```bat
cd examples\limiter_vst3
cmake -S . -B build -G "MinGW Makefiles" -DLIMITER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O CMake configura o output Windows em:

```text
build\out\limiter_vst3\limiter_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\limiter_vst3`.
2. Faça rescan.
3. Carregue **CV Limiter**.
4. Se o backend CV_GUI falhar, o controller retorna `nullptr` e a DAW usa o Generic Editor.
