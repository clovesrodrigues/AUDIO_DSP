# dc_blocker_vst3

Plugin VST3 individual para **DC Blocker** usando `CV_DSP/Filters/DCBlocker.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::filters::DCBlocker<float>
  -> saída de áudio
```

## Parâmetros

- `Cutoff`: 1 Hz a 200 Hz

## Build

```bat
cd examples\dc_blocker_vst3
cmake -S . -B build -G "MinGW Makefiles" -DDC_BLOCKER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\dc_blocker_vst3\dc_blocker_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\dc_blocker_vst3`.
2. Faça rescan.
3. Abra o plugin **CV DC Blocker**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
