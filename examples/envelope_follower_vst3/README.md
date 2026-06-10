# envelope_follower_vst3

Plugin VST3 individual para **CV Envelope Follower** usando `CV_DSP/Dynamics/EnvelopeFollower.hpp` e interface opcional via **CV_GUI**.

## Arquitetura

```text
Host / DAW
  -> VST3ParameterAdapter
  -> ParameterManager
  -> cvdsp::dynamics::EnvelopeFollower<float>
  -> saída de áudio
```

## Parâmetros

- `Mode`: Peak ou RMS
- `Attack`: 0.1 ms a 200 ms
- `Release`: 1 ms a 2000 ms
- `Output Gain`: -24 dB a +24 dB
- `Wet Mix`: 0% a 100%

## Build MinGW64

```bat
cd examples\envelope_follower_vst3
cmake -S . -B build -G "MinGW Makefiles" -DENVELOPE_FOLLOWER_VST3_ENABLE_CV_GUI=ON
mingw32-make -C build
```

O CMake configura o output Windows em:

```text
build\out\envelope_follower_vst3\envelope_follower_vst3.vst3
```

## Teste na DAW

1. Copie ou aponte o scanner VST3 da DAW para `build\out\envelope_follower_vst3`.
2. Faça rescan.
3. Carregue **CV Envelope Follower**.
4. Se o backend CV_GUI falhar, o controller retorna `nullptr` e a DAW usa o Generic Editor.
