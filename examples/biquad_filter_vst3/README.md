# biquad_filter_vst3

Plugin VST3 individual para **Biquad Filter** usando `CV_DSP/Filters/Biquad.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> ParameterManager
  -> cvdsp::filters::Biquad<float>
  -> saída de áudio
```

## Parâmetros

- `Mode`: LowPass, HighPass, BandPass, Notch, AllPass, PeakingEQ, LowShelf ou HighShelf
- `Frequency`: 20 Hz a 20 kHz
- `Q`: 0.1 a 20
- `Gain`: -24 dB a +24 dB

## Build

```bat
cd examples\biquad_filter_vst3
cmake -S . -B build -G "MinGW Makefiles" -DBIQUAD_FILTER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O output Windows é relativo ao build do plugin:

```text
build\out\biquad_filter_vst3\biquad_filter_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\biquad_filter_vst3`.
2. Faça rescan.
3. Abra o plugin **CV Biquad Filter**.
4. Se o CV_GUI estiver disponível, a janela Dear ImGui será criada; caso contrário, a DAW deve usar o editor genérico.
