# Spring Reverb VST3

`spring_reverb_vst3` is an individual CV_DSP VST3 integration for `cvdsp::reverb::SpringReverb<float>`.

## Parameters

| Parameter | Mapping |
| --- | --- |
| `Mix` | Wet/dry blend. |
| `Dwell` | Tank excitation / boing intensity. |
| `Tone` | Reverb tank tonal voicing. |

`Dwell` maps to spring tension and spring length to increase the audible boing/chirp intensity while keeping the UI consistent with the amp-style spring plugins.

## Architecture

```text
Host automation
  -> VST3ParameterAdapter
  -> cvdsp::manager::ParameterManager
  -> cvdsp::reverb::SpringReverb<float>
```

The process hot path copies host input to the output bus, creates a stack-only `cvdsp::AudioBufferView<float>`, and calls the DSP `processBlock()` method. No heap allocation, locks, or RTTI-dependent dispatch are used inside the audio block.

## Build with MinGW

From the repository root:

```bat
cmake -S examples/spring_reverb_vst3 -B build/spring_reverb_vst3 -G "MinGW Makefiles" -DSPRING_REVERB_VST3_ENABLE_CV_GUI=ON
cmake --build build/spring_reverb_vst3 --config Release
```

Or invoke `mingw32-make` directly after configuration:

```bat
cd build\spring_reverb_vst3
mingw32-make -j4
```

## Quick test

1. Copy or scan the generated VST3 from the build output.
2. Insert `spring_reverb_vst3` on a stereo audio track.
3. Open the CV_GUI editor or the host generic editor.
4. Adjust `Mix`, `Dwell`, and `Tone` while playing guitar or spring-friendly transient material.
