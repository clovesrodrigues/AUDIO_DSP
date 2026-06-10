# one_pole_filter_vst3

Plugin VST3 individual para **One Pole Filter** usando `CV_DSP/Filters/OnePoleFilter.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::filters::LowPassOnePole<float>
  -> saída de áudio
```

## Parâmetros

- `Mode`: LowPass ou HighPass
- `Cutoff`: 20 Hz a 20 kHz

## Build

```bat
cd examples\one_pole_filter_vst3
cmake -S . -B build -G "MinGW Makefiles" -DONE_POLE_FILTER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\one_pole_filter_vst3\one_pole_filter_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\one_pole_filter_vst3`.
2. Faça rescan.
3. Abra o plugin **CV One Pole Filter**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
