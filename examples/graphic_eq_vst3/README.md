# graphic_eq_vst3

VST3 Graphic EQ de 10 bandas usando `CV_DSP/EQ/GraphicEQ.hpp` e integração opcional com `CV_GUI`.

## Parâmetros

O plugin expõe ganho em dB para 10 bandas fixas:

31 Hz, 63 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz e 16 kHz.

Cada banda possui faixa de `-24 dB` a `+24 dB`.

## Build MinGW64

```bat
cd examples\graphic_eq_vst3
cmake -S . -B build -G "MinGW Makefiles" -DGRAPHIC_EQ_VST3_ENABLE_CV_GUI=ON
cmake --build build
```

Artefato esperado no Windows:

```text
build/VST3/graphic_eq_vst3.vst3/Contents/x86_64-win/graphic_eq_vst3.vst3
```

## Teste no REAPER

1. Adicione a pasta `build/VST3` ao path de VST3 do REAPER ou copie `graphic_eq_vst3.vst3` para sua pasta de plugins.
2. Faça rescan dos plugins.
3. Insira **CV Graphic EQ** em uma track.
4. Se `CV_GUI` estiver disponível, a janela Dear ImGui será criada. Caso contrário, o plugin retorna `nullptr` no editor e o host usa a GUI genérica de parâmetros.
