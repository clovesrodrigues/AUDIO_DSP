# ladder_filter_vst3

Plugin VST3 individual para **Ladder Filter** usando `CV_DSP/Filters/LadderFilter.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::filters::LadderFilter<float>
  -> saída de áudio
```

## Parâmetros

- `Cutoff`: 20 Hz a 20 kHz
- `Resonance`: 0 a 4
- `Drive`: 1x a 20x

## Build

```bat
cd examples\ladder_filter_vst3
cmake -S . -B build -G "MinGW Makefiles" -DLADDER_FILTER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\ladder_filter_vst3\ladder_filter_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\ladder_filter_vst3`.
2. Faça rescan.
3. Abra o plugin **CV Ladder Filter**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
