# Spectral Noise Reducer VST3 utility

This example is a VST3 front-end for `cvdsp::spectral::SpectralNoiseReducer`.
It is intentionally **not** under `examples/pedais/` because it is not a pedal:
it is a front-of-chain recording utility for learning and subtracting stationary
input noise before the musician builds a pedal/effect set.

## Mandatory controls

The host parameter list exposes the three required controls first:

1. **Perceber Ruido**: learns the current noise profile while the input is silent.
2. **Subtrair Ruidos**: applies the learned profile to incoming audio.
3. **Limpar Perfil**: clears the learned profile so a new room/cable/gain setup can
   be captured.

The plug-in also exposes `Output Gain`, `Presence Protect`, `Reduction Amount`,
`Spectral Floor`, `Max Reduction`, `Smoothing`, `Mix` and `Bypass`.

## Recommended recording flow

1. Insert this VST3 first in the track input chain.
2. Leave the microphone/guitar silent with the real recording gain.
3. Turn on **Perceber Ruido** for a few seconds.
4. Turn off **Perceber Ruido**.
5. Turn on **Subtrair Ruidos** and record through the cleaned signal.
6. Use **Limpar Perfil** whenever the cable, room, pickup, gain or interface changes.

## GUI fallback

The project is wired with a `SPECTRAL_NOISE_REDUCER_VST3_ENABLE_CV_GUI` CMake
option. When CV_GUI is unavailable, `createView()` returns `nullptr`, allowing
DAWs to show their native parameter editor fallback. The VST3 can now create the
shared `CV::GUI::ImGuiBackend` when that option is enabled, and the DSP already
exposes `fillGuiSnapshot(GuiSnapshot&)` plus `getBinFrequencyHz()` so the
dedicated spectrum panel can draw input/noise/output/reduction curves without
adding allocations to the audio path.

## Build

From the repository root:

```bash
cmake -S examples/spectral_noise_reducer_vst3 -B build/examples/spectral_noise_reducer_vst3
cmake --build build/examples/spectral_noise_reducer_vst3
```

On Linux the module is emitted under:

```text
build/examples/spectral_noise_reducer_vst3/VST3/spectral_noise_reducer_vst3.vst3/Contents/x86_64-linux/
```

The learned noise profile is runtime state only in this foundation example; save
or export/import of profiles should be added later if session persistence is
required.
