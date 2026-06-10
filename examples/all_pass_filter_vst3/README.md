# all_pass_filter_vst3

Plugin VST3 individual para **All Pass Filter** usando `CV_DSP/Filters/AllPassFilter.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::filters::AllPassFilter<float>
  -> saída de áudio
```

## Parâmetros

- `Delay Samples`: 1 a 4096 samples
- `Feedback`: -0.999 a +0.999

## Build

```bat
cd examples\all_pass_filter_vst3
cmake -S . -B build -G "MinGW Makefiles" -DALL_PASS_FILTER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\all_pass_filter_vst3\all_pass_filter_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\all_pass_filter_vst3`.
2. Faça rescan.
3. Abra o plugin **CV All Pass Filter**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
