# CV GM Instrument Lite VST3

`CV GM Instrument Lite` is a minimal single-instrument SoundFont VST3 for playing `.sf2` presets from Reaper MIDI items or from a live MIDI keyboard/sequencer.

## Current status

Post-test correction: the SoundFont and Instrument lists are now refreshed from the real sibling `CV_GM_Instrument_Lite_SoundFonts/` folder instead of relying on a fixed General MIDI instrument list. The first list should show actual `.sf2` filenames, and the second list should show the real presets extracted from the selected `.sf2` when TinySoundFont is available.

## Fixed SoundFont folder contract

The final plugin is expected to have a sibling folder with this exact name:

```text
CV_GM_Instrument_Lite.vst3
CV_GM_Instrument_Lite_SoundFonts/
```

The user manually places `.sf2` files in that folder:

```text
CV_GM_Instrument_Lite_SoundFonts/
    GeneralUser GS.sf2
    FluidR3_GM.sf2
    another_bank.sf2
```

An optional plugin data/cache subfolder is reserved inside the SoundFonts folder:

```text
CV_GM_Instrument_Lite_SoundFonts/
    _CV_GM_Instrument_Lite_Data/
```

## Manual installation and Reaper use

1. Build the example VST3 target with CMake.
2. Copy or locate the generated `CV_GM_Instrument_Lite.vst3` bundle/file in the folder where Reaper scans VST3 plugins.
3. Create `CV_GM_Instrument_Lite_SoundFonts/` next to that final `.vst3` bundle/file.
4. Copy your chosen `.sf2` banks into `CV_GM_Instrument_Lite_SoundFonts/` manually.
5. Open Reaper, rescan plugins if needed, insert `CV GM Instrument Lite` as a VSTi, press `Rescan`, choose a SoundFont, choose an instrument preset, and play either a MIDI item or a live MIDI keyboard/sequencer.

## SoundFont scanning

The scanner resolves the expected folder from a `CV_GM_Instrument_Lite.vst3` bundle path, scans only regular `.sf2` files, accepts `.sf2` case-insensitively, ignores non-SoundFont files, and sorts the list by filename for stable UI ordering.

## Real preset list

When TinySoundFont is available and a bank loads, the engine rebuilds a preset list from the selected `.sf2`. Each entry stores the TinySoundFont preset index, raw preset name, and a formatted label. Selecting a different preset stops active notes first so new notes use the newly selected instrument cleanly. The controller also loads the selected `.sf2` on the UI side to populate the Instrument list with real SoundFont presets instead of a fixed General MIDI name table.

## Interface

```text
SoundFont:   [selected .sf2 file ▼]
Instrument:  [GM preset / selected preset ▼]
Rescan

Volume
Bass
Mid
Treble
Room
```

`Room` means the reverb amount in the existing CV_DSP hall-style reverb path. It is named `Room` in the user interface to stay simple for performance use.

## Signal path

```text
MIDI In -> TinySoundFont Engine -> Volume/MIDI expression -> 3-Band EQ -> Hall Reverb amount (Room) -> Stereo Out
```

## Scope for version 1

- Single selected `.sf2` at a time.
- Single selected instrument/preset at a time.
- Polyphonic playback.
- Reaper MIDI editor playback.
- Live keyboard/sequencer playback.
- Volume, Bass, Mid, Treble, and Room only.
- MIDI velocity, sustain CC64, pitch bend, all-notes-off/all-sounds-off, MIDI Volume CC7, Expression CC11, Reverb Send CC91, and keyboard EQ CC hooks for bass/mid/treble.
- CV_GUI custom editor when enabled in CMake, with DAW generic editor fallback. The CV_GUI view now starts larger and advertises resize support to hosts that allow embedded VST3 editor resizing.

## Explicit limitations for this closed first cycle

- No 16-channel multitimbral GM workstation mode.
- No GM drum-channel special handling.
- No automatic internet downloads.
- No bundled or embedded `.sf2` files.
- No compressor, drive, humanize layer, physical modeling, or synthetic bass voice in this project.
- No sampler editor, waveform editor, SF2 authoring, or advanced articulation engine.

## TinySoundFont audit

The CMake project expects TinySoundFont to live under `backends/TinySoundFont/` with `tsf.h`, meaning the expected repository path is `backends/TinySoundFont/tsf.h`. On case-sensitive systems, the directory and filename must match exactly. If `tsf.h` is not found there, the scanner/foundation still builds in fallback mode and CMake prints a warning. When `tsf.h` is present, `SoundFontEngine` compiles the TinySoundFont-backed load/render path.

## SoundFont suggestions

The plugin does not download or bundle `.sf2` files. Put SoundFonts in `CV_GM_Instrument_Lite_SoundFonts/` manually. Possible places to research include GeneralUser GS, MuseScore General, and FluidR3 GM. Always verify the license before redistributing any `.sf2` with your own plugin/package.

## Build and smoke test

Configure this example directly with CMake, using the bundled VST3 SDK expected at `backends/vst3sdk`:

```sh
cmake -S examples/cv_gm_instrument_lite_vst3 -B build/cv_gm_instrument_lite_vst3
cmake --build build/cv_gm_instrument_lite_vst3
./build/cv_gm_instrument_lite_vst3/cv_gm_instrument_lite_foundation_smoke
```

If TinySoundFont is available and you want the smoke test to try loading a real bank, set:

```sh
CV_GM_INSTRUMENT_LITE_TEST_SF2=/path/to/bank.sf2 ./build/cv_gm_instrument_lite_vst3/cv_gm_instrument_lite_foundation_smoke
```

CV_GUI is enabled by default for this project foundation:

```sh
cmake -S examples/cv_gm_instrument_lite_vst3 -B build/cv_gm_instrument_lite_vst3 -DCV_GM_INSTRUMENT_LITE_ENABLE_CV_GUI=ON
```

## Cycle closure

This is the last prompt of the first GM Instrument Lite cycle. Do not keep adding features by simply saying `prossiga`; first test this closed scope in Reaper with real SoundFonts, then start a new plan only if the musical result points to a specific missing feature or bug.
