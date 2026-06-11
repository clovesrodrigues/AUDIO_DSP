# Spectral Noise Reducer VST3 utility

This example is a VST3 front-end for `cvdsp::spectral::RealtimeNoiseReducer`.
It is intentionally **not** under `examples/pedais/` because it is not a pedal:
it is a front-of-chain recording utility for learning and subtracting stationary
input noise before the musician builds a pedal/effect set.

## Minimal public controls

The VST3 intentionally exposes only the controls needed for the recording
workflow:

1. **Perceber Ruido**: starts a fresh noise-profile capture while the input is silent.
2. **Subtrair Ruido**: applies the learned profile to incoming audio.
3. **Ganho**: post-reduction output gain.
4. **Presenca**: protects the guitar/vocal presence range from sounding dull.
5. **Smooth**: smooths gain movement to reduce zippering and spectral chatter.

Advanced spectral-safety values and banded gain smoothing are internal defaults
in the clean real-time core. They are not registered as host parameters because this utility should
behave like a simple front-of-chain recording tool, not a laboratory spectral
editor.

## Recommended recording flow

1. Insert this VST3 first in the track input chain.
2. Leave the microphone/guitar silent with the real recording gain.
3. Turn on **Perceber Ruido** for a few seconds.
4. Turn off **Perceber Ruido**.
5. Turn on **Subtrair Ruido** and record through the cleaned signal.
6. Turning **Perceber Ruido** on again starts a fresh profile if the cable, room,
   pickup, gain or interface changes.

When **Perceber Ruido** and **Subtrair Ruido** are both off, the VST3 takes a
block-level pass-through path and avoids calling the FFT reducer per sample.
Toggling either switch also flushes only the overlap/latency history, not the
learned profile, so old FFT tails cannot leak into the next capture/reduction
state.

The processor reports `1024` samples of plugin latency compensation (PDC), matching
the realtime FFT size, so REAPER can keep duplicated tracks and parallel routes
aligned instead of mixing a delayed spectral path with an uncompensated dry path.

## CV_GUI editor and fallback

The project enables `SPECTRAL_NOISE_REDUCER_VST3_ENABLE_CV_GUI` by default on
Windows so REAPER can open the CV_GUI editor instead of the native slider-only
parameter fallback. The CV_GUI editor names this window **CV Spectral Noise
Reducer**, draws boolean parameters such as **Perceber Ruido** and
**Subtrair Ruido** as click-only ON/OFF buttons, and keeps the remaining
continuous controls as sliders. This avoids noisy drag gestures on the capture
and validation switches while preserving VST3 `beginEdit` / `performEdit` /
`endEdit` automation gestures.

If CV_GUI is unavailable, `createView()` still returns `nullptr` and the DAW can
show its native parameter editor. The DSP already exposes
`fillGuiSnapshot(GuiSnapshot&)` plus `getBinFrequencyHz()` for a spectrum panel
without adding allocations to the audio path.

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

Each VST3 instance owns its own stereo pair of `RealtimeNoiseReducer` objects,
so learned profiles, FFT buffers, overlap-add rings and gain-smoothing history
are isolated per track/instance. The learned noise profile is runtime state only
in this foundation example; save or export/import of profiles should be added
later if session persistence is required.
