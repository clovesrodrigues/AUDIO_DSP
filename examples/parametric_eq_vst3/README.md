# parametric_eq_vst3

VST3 Parametric EQ de 5 bandas usando `CV_DSP/EQ/ParametricEQ.hpp` e integração opcional com `CV_GUI`.

## Parâmetros

Cada uma das 5 bandas expõe:

- `Type`: LowPass, HighPass, Peaking, LowShelf, HighShelf, Notch
- `Freq`: 20 Hz a 20 kHz
- `Q`: 0.10 a 10.0
- `Gain`: -24 dB a +24 dB

## Build MinGW64

```bat
cd examples\parametric_eq_vst3
cmake -S . -B build -G "MinGW Makefiles" -DPARAMETRIC_EQ_VST3_ENABLE_CV_GUI=ON
cmake --build build
```

Artefato esperado no Windows:

```text
build/VST3/parametric_eq_vst3.vst3/Contents/x86_64-win/parametric_eq_vst3.vst3
```

## Teste no REAPER

1. Adicione a pasta `build/VST3` ao path de VST3 do REAPER ou copie `parametric_eq_vst3.vst3` para sua pasta de plugins.
2. Faça rescan dos plugins.
3. Insira **CV Parametric EQ** em uma track.
4. Se `CV_GUI` estiver disponível, a janela Dear ImGui será criada. Caso contrário, o plugin retorna `nullptr` no editor e o host usa a GUI genérica de parâmetros.
