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

Post-cycle sound triage. The plugin still follows the Prompt 9 closure plan, but the default sound has been corrected to start cleaner and less synthetic: drive, finger noise, and room are off by default, compression and humanization are conservative, and the tone filter is warmer to avoid the harsh short-circuit character reported during listening.

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
8. Keep `Drive`, `Finger Noise`, and `Room` at zero first; only add them after the clean bass tone is working in the mix.

## Safe default sound-shaping range

- `Tone`: start around 0.35 to 0.45; raise only if the bass is too dark.
- `Attack`: 2 ms to 8 ms for most keyboard/sequenced bass lines.
- `Velocity Sensitivity`: 0.50 to 0.80 when the MIDI part has meaningful velocity variation.
- `Compression`: 0.10 to 0.30 for stable level without crushing the transient.
- `Drive`: start at 0.00; try 0.03 to 0.10 only if the clean tone needs harmonic weight.
- `Finger Noise`: start at 0.00; use 0.02 to 0.08 for a subtle attack detail.
- `Humanize`: 0.00 to 0.10 for small movement without random-sounding notes.
- `Room`: start at 0.00 for bass; keep below 0.10 unless it is a special effect.

## Listening correction

The first completed pass was too synthetic and could sound like electrical crackle when `Finger Noise`, `Drive`, and ambience were active by default. The current defaults intentionally prioritize a cleaner direct bass tone over obvious effects. Treat noise, drive, and room as optional mix controls, not as part of the base sound.

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
