# CV Spectrum Analyzer VST3

Exemplo VST3 usando `CV_DSP/Spectral/SpectrumAnalyzer.hpp` e a classe `cvdsp::spectral::SpectrumAnalyzer<float, N>`.

## Parâmetros expostos

- `Windowing`: Rectangular, Hann, Hamming, Blackman, Blackman-Harris ou Kaiser.
- `FFT Size`: 512, 1024 ou 2048 samples.

## Processamento e visualização

O plugin preserva o áudio de entrada na saída e usa uma cópia mono derivada de L/R para preencher buffers espectrais internos. A implementação respeita layout estéreo VST3: processa L/R quando há dois canais, repassa mono sem alteração e copia canais extras.

Quando `CV_GUI` está disponível, a view Dear ImGui expõe os controles `Windowing` e `FFT Size`; os buffers de magnitude são mantidos no processador para uma visualização espectral dedicada quando o backend gráfico evoluir para receber dados de análise.

## Build MinGW64

```bash
cmake -S examples/spec_analyzer_vst3 -B build/spec_analyzer_vst3 -G "MinGW Makefiles"
cmake --build build/spec_analyzer_vst3 --parallel
```

Artefato Windows esperado:

```text
build/spec_analyzer_vst3/out/spec_analyzer_vst3/spec_analyzer_vst3.vst3
```
