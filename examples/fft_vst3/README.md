# CV FFT VST3

Exemplo VST3 usando `CV_DSP/Spectral/FFT.hpp` e a classe `cvdsp::spectral::FFT<float>`.

## Parâmetros expostos

- `Windowing`: Rectangular, Hann, Hamming, Blackman, Blackman-Harris ou Kaiser.
- `FFT Size`: 512, 1024 ou 2048 samples.

## Processamento e visualização

O plugin preserva o áudio de entrada na saída e usa uma cópia mono derivada de L/R para preencher buffers espectrais internos. A implementação respeita layout estéreo VST3: processa L/R quando há dois canais, repassa mono sem alteração e copia canais extras.

Quando `CV_GUI` está disponível, a view Dear ImGui expõe os controles `Windowing` e `FFT Size`; os buffers de magnitude são mantidos no processador para uma visualização espectral dedicada quando o backend gráfico evoluir para receber dados de análise.

## Build MinGW64

```bash
cmake -S examples/fft_vst3 -B build/fft_vst3 -G "MinGW Makefiles"
cmake --build build/fft_vst3 --parallel
```

Artefato Windows esperado:

```text
build/fft_vst3/out/fft_vst3/fft_vst3.vst3
```
