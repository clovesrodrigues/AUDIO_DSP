# flanger_vst3

Plugin VST3 individual para **CV Flanger** usando `CV_DSP/Effects/Flanger.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> VST3ParameterAdapter
  -> ParameterManager
  -> cvdsp::Flanger<float>
  -> saída de áudio
```

> Observação: o header atual declara `Flanger` diretamente em `namespace cvdsp`.

## Parâmetros

- `Rate`: 0.01 Hz a 20 Hz
- `Depth`: 0.0 a 1.0
- `Feedback`: -0.99 a +0.99
- `Mix`: 0.0 a 1.0

## Build

```bat
cd examples\flanger_vst3
cmake -S . -B build -G "MinGW Makefiles" -DFLANGER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\flanger_vst3\flanger_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\flanger_vst3`.
2. Faça rescan.
3. Abra o plugin **CV Flanger**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
