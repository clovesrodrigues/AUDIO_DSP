# CV GM Instrument Lite prompt plan

This plan records the new minimal SoundFont-based General MIDI instrument cycle. The goal is to avoid the previous synthesis experiment problem: this plugin should play real `.sf2` presets and expose only the controls needed for a practical Reaper instrument.

## Fixed project decisions

- Project name: `CV GM Instrument Lite`.
- Plugin output name: `CV_GM_Instrument_Lite.vst3`.
- Required sibling SoundFont folder: `CV_GM_Instrument_Lite_SoundFonts/`.
- Optional data/cache folder inside it: `_CV_GM_Instrument_Lite_Data/`.
- Sound engine target: TinySoundFont.
- SoundFonts are supplied manually by the user; the plugin does not download `.sf2` files.
- Instrument list must come from the real presets inside the selected `.sf2`.
- The plugin is single-instrument, polyphonic, and not multitimbral in this first cycle.
- `Room` means a reverb amount using the existing CV_DSP hall-style reverb path, not a separate room-modeling lab.
- Use `CV_GUI` for the custom editor when enabled in CMake; fall back to the DAW generic editor if disabled or unavailable.

## Prompt sequence

1. **Audit and foundation** — create the VST3 project skeleton, confirm TinySoundFont location, define the SoundFonts folder contract, add initial docs, and build a silent instrument foundation.
2. **SoundFont folder scanner** — resolve the sibling `CV_GM_Instrument_Lite_SoundFonts/` path, scan `.sf2` files, and expose rescan/list state.
3. **TinySoundFont minimal load/render** — integrate TinySoundFont, load a selected `.sf2`, and render a test note outside the full MIDI path.
4. **Real preset list** — enumerate real presets from the selected `.sf2`, sort by bank/program, and select instrument safely.
5. **Real-time MIDI playback** — handle Note On/Off/Velocity from Reaper MIDI items and a live keyboard/sequencer with fixed internal polyphony.
6. **MIDI performance controls** — add sustain CC64, pitch bend, all-notes-off, and record the planned keyboard controls for expression/volume/EQ mapping.
7. **Minimal audio controls** — add Volume, Bass, Mid, Treble, and Room using CV_DSP EQ/reverb blocks only.
8. **CV_GUI editor** — build the required CV_GUI editor with SoundFont listbox, Instrument listbox, Rescan, and the five sliders, with DAW fallback.
9. **State/cache portability** — save/restore selected `.sf2`, selected preset, parameters, and optional sibling-folder cache data.
10. **Documentation/tests/closure** — document installation, SoundFont folder setup, Reaper use, SoundFont source suggestions, limitations, and stop the first cycle.

## Current prompt

Prompt 10/10 is the current and final step of this first cycle. It documents installation, Reaper use, the SoundFont folder contract, TinySoundFont expectations, limitations, and closes the cycle to avoid feature creep.

## Cycle closure rule

After Prompt 10/10, do not continue adding features by repeatedly saying `prossiga`. First test `CV GM Instrument Lite` in Reaper with real `.sf2` banks in `CV_GM_Instrument_Lite_SoundFonts/`. If the musical test reveals a concrete missing behavior, start a new prompt plan for that next cycle.

## Do not add in this cycle

- 16-channel multitimbral playback.
- GM drum channel 10.
- Automatic internet download of SoundFonts.
- Embedded `.sf2` in the plugin binary.
- Compressor, drive, humanize, synthetic noise, physical modeling, or custom sample editing.
