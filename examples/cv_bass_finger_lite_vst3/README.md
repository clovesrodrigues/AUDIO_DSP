# CV Bass Finger Lite VST3

`CV Bass Finger Lite` is the planned lightweight monophonic electric bass finger instrument for CV_DSP.

This stage creates the first playable lightweight instrument path:

- stereo audio output;
- MIDI/event input bus;
- 32-bit floating-point processing support;
- first lightweight bass voice rendered from MIDI notes;
- no sampler, convolution, physical modeling, heavy reverb, or custom GUI.

## Planned signal path

```text
MIDI In -> Mono Note Tracker -> Bass Finger Voice -> Dynamic Filter -> Drive -> Compressor -> 3-Band EQ -> Optional Room -> Stereo Out
```

## Current status

Stage 9 closure and usage guide. The plugin translates VST3 note events into neutral CV_DSP MIDI events, tracks a fixed-capacity monophonic last-note-priority stack, renders a low-CPU bass voice made from a triangle oscillator, sine sub oscillator, amplitude ADSR, velocity-aware gain, dynamic low-pass tone filter, light drive, compressor, 3-band EQ, subtle attack/release finger noise, and deterministic humanization, then can add a short low-CPU room ambience at the output.

## Parameters

- `Tone`: adjusts the dynamic low-pass filter from warm/dark to brighter.
- `Attack`: controls the amplitude envelope attack time in milliseconds.
- `Velocity Sensitivity`: controls how strongly MIDI velocity affects level and filter brightness.
- `Compression`: adds lightweight dynamic control and makeup gain.
- `Drive`: adds subtle tanh saturation before compression.
- `Bass`: low-shelf EQ gain in dB.
- `Mid`: peaking EQ gain in dB.
- `Treble`: high-shelf EQ gain in dB.
- `Finger Noise`: controls subtle attack and release noise amount.
- `Humanize`: adds deterministic per-note micro-variation to gain and filter brightness.
- `Room`: blends in a short low-CPU room ambience after the bass voice.
- `Output Gain`: trims the instrument output in dB.

## Prompt cycle

This example is at **Prompt 9**, the final prompt of the first CV Bass Finger Lite cycle. The detailed prompt checklist is recorded in `PROMPT_PLAN.md` so future work starts intentionally instead of continuing to add features indefinitely.

## Suggested Reaper starting point

1. Build the VST3 target with CMake.
2. Copy or point Reaper to the generated `cv_bass_finger_lite_vst3.vst3` bundle for your platform.
3. Insert it as a virtual instrument on a MIDI track.
4. Start with MIDI notes in the electric-bass range, around E1 to G3.
5. Keep the part monophonic for the intended finger-bass behavior.
6. Use velocity variation in the MIDI item before reaching for more effects.
7. Use `Tone`, `Velocity Sensitivity`, `Finger Noise`, and `Humanize` as the main realism controls.
8. Keep `Room` low for bass parts; the default is intended as a short ambience, not a large reverb.

## Safe default sound-shaping range

- `Tone`: 0.45 to 0.70 for a usable finger-bass range.
- `Attack`: 1.5 ms to 6 ms for most keyboard/sequenced bass lines.
- `Velocity Sensitivity`: 0.60 to 0.90 when the MIDI part has meaningful velocity variation.
- `Compression`: 0.25 to 0.50 for stable level without crushing the transient.
- `Drive`: 0.05 to 0.18 for subtle harmonic weight.
- `Finger Noise`: 0.10 to 0.30 for audible but not exaggerated articulation.
- `Humanize`: 0.10 to 0.25 for small movement without random-sounding notes.
- `Room`: 0.00 to 0.18 for bass; use higher values only for special effect.

## Stop point for this cycle

This first cycle is intentionally complete once the plugin builds, receives MIDI, produces a playable monophonic bass tone, exposes the practical shaping controls, and documents how to start using it in Reaper. New features should begin as a separate prompt cycle after testing the instrument musically.

## Build

Configure this example directly with CMake, using the bundled VST3 SDK expected at `backends/vst3sdk`:

```sh
cmake -S examples/cv_bass_finger_lite_vst3 -B build/cv_bass_finger_lite_vst3
cmake --build build/cv_bass_finger_lite_vst3
./build/cv_bass_finger_lite_vst3/cv_bass_finger_lite_midi_smoke
./build/cv_bass_finger_lite_vst3/cv_bass_finger_lite_voice_smoke
```
