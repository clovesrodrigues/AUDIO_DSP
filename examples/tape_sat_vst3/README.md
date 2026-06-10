# Tape Sat VST3

Exemplo VST3 usando `CV_DSP/Saturation/TapeSaturation.hpp` e a classe `cvdsp::saturation::TapeSaturation<float>`.

## Parâmetros expostos

- `Drive`: intensidade de saturação do estágio DSP.
- `Bias`: deslocamento DC/asimetria antes ou dentro do estágio de saturação.
- `Mix`: mistura dry/wet aplicada fora do hot path de alocação dinâmica.

## GUI

O projeto tenta habilitar `CV_GUI` automaticamente em Windows com `-DTAPE_SAT_VST3_ENABLE_CV_GUI=ON`.
Se o backend Win32/OpenGL3 não estiver disponível, o controller retorna `nullptr` e a DAW usa o editor genérico de parâmetros.

## Build MinGW64

```bash
cmake -S examples/tape_sat_vst3 -B build/tape_sat_vst3 -G "MinGW Makefiles"
cmake --build build/tape_sat_vst3 --parallel
```

Em Windows, o artefato é emitido em:

```text
build/tape_sat_vst3/out/tape_sat_vst3/tape_sat_vst3.vst3
```

Copie o `.vst3` para a pasta de plugins da DAW ou aponte o scanner do host para esse diretório de saída.
