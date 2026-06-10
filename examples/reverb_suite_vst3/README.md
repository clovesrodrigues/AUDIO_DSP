# CV_DSP Reverb Suite VST3

`reverb_suite_vst3` is a unified VST3 example plugin for the seven CV_DSP reverb engines:

1. Room Reverb
2. Hall Reverb
3. Plate Reverb
4. Spring Reverb
5. Twin Reverb DSP
6. Deluxe Reverb DSP
7. Super Reverb DSP

The processor owns all seven algorithms and routes the current block to one selected model using the discrete **Model** parameter. Shared global controls are mapped to the nearest meaningful parameter on each algorithm.

## Parameters

| Parameter | Purpose |
| --- | --- |
| `Model` | Discrete reverb algorithm selector. Exposed as a combo/list control by CV_GUI / generic VST3 hosts. |
| `Mix` | Global wet/dry blend. |
| `Decay` | Decay, RT60, spring length, or reverb amount depending on the selected model. |
| `Tone` | Tone, brightness, or inverse damping depending on the selected model. |
| `PreDelay` | Pre-delay for models that expose it; harmlessly ignored by spring/tank models. |
| `Width/Dwell` | Stereo width for spatial algorithms and dwell/tension for spring/tank algorithms. |

## Architecture

The signal flow follows the intended CV_DSP integration pattern:

```text
Host automation
  -> VST3ParameterAdapter
  -> cvdsp::manager::ParameterManager
  -> selected CV_DSP reverb core
```

The audio hot path avoids dynamic allocation, locks, and RTTI-dependent dispatch. The selected algorithm is routed with a `switch` over the current integer `Model` parameter.

## UI

When built on Windows with `REVERB_SUITE_VST3_ENABLE_CV_GUI=ON`, the plugin links `CV_GUI` and exposes a Dear ImGui editor. The first control is the discrete **Model** combo followed by sliders for the shared global controls.

If CV_GUI is disabled or the GUI view cannot be created, `createView()` returns `nullptr` so the host can silently fall back to its generic parameter editor.

## Build with MinGW

From the repository root:

```bat
cmake -S examples/reverb_suite_vst3 -B build/reverb_suite_vst3 -G "MinGW Makefiles" -DREVERB_SUITE_VST3_ENABLE_CV_GUI=ON
cmake --build build/reverb_suite_vst3 --config Release
```

Or invoke `mingw32-make` directly after configuration:

```bat
cd build\reverb_suite_vst3
mingw32-make -j4
```

The Windows artifact is written under:

```text
build/reverb_suite_vst3/out/reverb_suite_vst3/reverb_suite_vst3.vst3
```

## Quick test

1. Copy the generated `.vst3` bundle/file to a VST3 scan path, or add the build output folder to your host's plugin search paths.
2. Rescan plugins in a VST3 host.
3. Insert **reverb_suite_vst3** on a stereo audio track.
4. Open the editor or the host generic editor.
5. Change **Model** between Room, Hall, Plate, Spring, Twin, Deluxe, and Super while adjusting `Mix`, `Decay`, `Tone`, `PreDelay`, and `Width/Dwell`.

## Notes

- This example uses only project-relative paths in CMake.
- Only 32-bit float VST3 processing is enabled (`kSample32`), matching the existing CV_DSP examples.
- The plugin is intended as a clear integration example rather than a preset-managed commercial product.
