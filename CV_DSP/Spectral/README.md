# CV_DSP Spectral

This folder contains frequency-domain building blocks for analysis and spectral
processing. These modules are header-only C++20 components intended to remain
independent from VST3, JUCE, CLAP, iPlug2 or any other host SDK.

## Modules

- `FFT.hpp`: in-place complex FFT engine for the supported fixed transform sizes.
- `STFT.hpp`: short-time Fourier transform helpers for frame-based analysis.
- `WindowFunctions.hpp`: static window generation helpers such as Hann, Hamming
  and Blackman-style windows.
- `SpectrumAnalyzer.hpp`: lightweight spectral analysis support for meters and
  GUIs.
- `SpectralNoiseReducer.hpp`: full frame/streaming spectral utility with GUI
  snapshots for experiments and visualization.
- `RealtimeNoiseReducer.hpp`: minimal profile learner/subtractor intended for the
  VST3 recording utility; it keeps only the learn/subtract/gain/presence/smooth
  workflow and uses frozen power profiles, 50% overlap and banded gain decisions
  to reduce CPU and robotic artifacts.

## SpectralNoiseReducer is not a pedal

`SpectralNoiseReducer` is a **noise perception / noise reduction DSP utility**.
It should normally be inserted at the beginning of a recording chain, before
pedals, amplifiers, reverbs or modulation effects. The goal is to learn the
stationary noise floor from the real input setup and subtract that profile while
recording.

Typical sources it targets:

- single-coil hum and electrical hum;
- microphone preamp hiss;
- cable noise;
- bad grounding noise;
- computer fan or room noise that stays mostly stationary;
- interface/input noise captured while the instrument is silent.

It is not a replacement for performance cleanup, editing or broadband mastering
restoration. It works best when the noise is reasonably stable and the captured
profile contains only unwanted background noise.

## Mandatory user workflow

The user-facing VST3 should expose two prominent workflow buttons plus the three
small tone/safety controls documented below:

1. **Perceber Ruído** / `setLearnNoiseEnabled(bool)`
   - Starts learning the current noise profile.
   - The VST3 clears the previous profile automatically when this control is
     switched on, so the musician does not need a separate public clear button.
   - Use while the instrument/microphone is silent but the real gain staging is
     active.
2. **Subtrair Ruído** / `setSubtractNoiseEnabled(bool)`
   - Applies the learned profile to the incoming signal.
   - Use after a profile is ready to clean the recording input.

`triggerClearProfile()` remains available in the DSP core for tests and custom
adapters, but the minimal VST3 hides it to keep the workflow close to
"learn, validate/subtract, play".

Recommended session flow:

```text
1. Put the plugin/DSP first in the chain.
2. Leave the instrument/microphone silent with the real recording gain.
3. Turn on Perceber Ruído for a short capture window.
4. Turn off Perceber Ruído after the profile is ready.
5. Turn on Subtrair Ruído and record normally.
6. Turn on Perceber Ruído again to capture a fresh profile after changing cable,
   pickup, microphone, gain, room, interface or sample rate.
```

When both Learn and Subtract are disabled, the streaming API is designed to be a
true pass-through so A/B checks do not alter the signal.

## Public controls for the VST3 utility

The recording utility should expose only five public controls:

- `Perceber Ruido`: learn a fresh noise profile from silence.
- `Subtrair Ruido`: apply the learned profile.
- `Ganho`: post-reduction output gain.
- `Presenca`: protect the guitar/vocal presence region from dulling.
- `Smooth`: temporal smoothing of attenuation changes.

The VST3 uses `RealtimeNoiseReducer`, not the heavier snapshot-oriented
`SpectralNoiseReducer`, so other spectral safety constants and the banded gain
optimizer remain internal defaults. Each reducer object owns its FFT engine, overlap-add rings, learned
profile and gain history; VST3 processors should therefore instantiate one
reducer per audio channel and per plug-in instance, never a shared/static
reducer.

## APIs

`SpectralNoiseReducer` supports two processing surfaces:

- `processSpectrum(...)`: frame-domain API for callers that already manage FFT or
  STFT frames.
- `processSample(...)` / `processBlock(AudioBufferView<T>)`: streaming time-domain
  API using fixed-size STFT buffers and overlap-add internally.

Both APIs share the same learned profile, snapshots and reduction parameters.

## GUI / spectrum visualization plan

The reducer stores fixed-size snapshots suitable for future GUI integration:

- `getInputSpectrum()`: current input magnitude per bin.
- `getNoiseProfile()`: learned average noise magnitude per bin.
- `getOutputSpectrum()`: output magnitude after reduction.
- `getReductionCurve()`: effective attenuation curve per bin.
- `fillGuiSnapshot(GuiSnapshot&)`: precomputes dB and normalized `[0, 1]`
  display arrays for input, learned noise, output and reduction curves.
- `getBinFrequencyHz(bin)`: maps bin indices to frequency labels for spectrum
  grids and cursor readouts.

A CV_GUI-based view should show at least:

```text
Before learning: live input spectrum, with the noise visibly above the floor.
After learning: frozen/colored noise profile overlay.
During subtract: input spectrum vs reduced output spectrum, plus reduction curve.
```

For real-time safety, the audio thread should only update fixed-size snapshots.
The UI thread should copy/read those snapshots without forcing allocation, file
I/O or locks in the audio path. `CV_GUI/Spectral/SpectralNoiseReducerPanel.hpp`
provides the first Dear ImGui panel helper for drawing those normalized curves.

## Example

A standalone smoke/tutorial example is available at:

- `examples/spectral_noise_reducer_dsp`

It learns a synthetic hum/hiss profile, enables subtraction, validates finite
output, checks RMS reduction, verifies spectral snapshots and tests profile
clearing plus hard bypass.
