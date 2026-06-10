# Hall Reverb VST3

`hall_reverb_vst3` is an individual CV_DSP VST3 integration for `cvdsp::reverb::HallReverb<float>`.

## Parameters

| Parameter | Mapping |
| --- | --- |
| `Mix` | Wet/dry blend. |
| `Decay` | Main decay/RT60/length control for this reverb model. |
| `Damping` | High-frequency damping control. |
| `PreDelay` | Pre-delay in milliseconds where supported by the underlying DSP. |

`Decay` maps to RT60 and hall size; `PreDelay` is forwarded to the hall pre-delay stage.

## Architecture

```text
Host automation
  -> VST3ParameterAdapter
  -> cvdsp::manager::ParameterManager
  -> cvdsp::reverb::HallReverb<float>
```

The process hot path copies host input to the output bus, creates a stack-only `cvdsp::AudioBufferView<float>`, and calls the DSP `processBlock()` method. No heap allocation, locks, or RTTI-dependent dispatch are used inside the audio block.

## Build with MinGW

From the repository root:

```bat
cmake -S examples/hall_reverb_vst3 -B build/hall_reverb_vst3 -G "MinGW Makefiles" -DHALL_REVERB_VST3_ENABLE_CV_GUI=ON
cmake --build build/hall_reverb_vst3 --config Release
```

Or invoke `mingw32-make` directly after configuration:

```bat
cd build\hall_reverb_vst3
mingw32-make -j4
```

## Quick test

1. Copy or scan the generated VST3 from the build output.
2. Insert `hall_reverb_vst3` on a stereo audio track.
3. Open the CV_GUI editor or the host generic editor.
4. Adjust `Mix`, `Decay`, `Damping`, and `PreDelay` while playing audio.
