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
- `SpectralNoiseReducer.hpp`: recording-chain noise profile learner/subtractor
  for reducing stationary background noise before pedal/effect processing.

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

The user-facing UI should always expose these three controls prominently:

1. **Perceber Ruído** / `setLearnNoiseEnabled(bool)`
   - Enables/disables learning the current noise profile.
   - Use while the instrument/microphone is silent but the real gain staging is
     active.
2. **Subtrair Ruídos** / `setSubtractNoiseEnabled(bool)`
   - Enables/disables applying the learned profile to the incoming signal.
   - Use after a profile is ready to clean the recording input.
3. **Limpar Perfil** / `triggerClearProfile()`
   - Clears the learned profile and resets the learn frame count.
   - Use whenever the input chain changes: cable, pickup, mic, gain, room,
     interface, sample rate or noise source.

Recommended session flow:

```text
1. Put the plugin/DSP first in the chain.
2. Leave the instrument/microphone silent with the real recording gain.
3. Turn on Perceber Ruído for a short capture window.
4. Turn off Perceber Ruído after the profile is ready.
5. Turn on Subtrair Ruídos and record normally.
6. Use Limpar Perfil before learning a new input/noise profile.
```

When both Learn and Subtract are disabled, the streaming API is designed to be a
true pass-through so A/B checks do not alter the signal.

## Advanced controls

These controls should be available below the three mandatory buttons, preferably
as an "Advanced" section for users who need finer tuning:

- `Output Gain`: post-reduction gain compensation.
- `Presence Protect`: protects part of the guitar/vocal presence region so heavy
  subtraction does not make the source too dull.
- `Reduction Amount`: overall aggressiveness of the learned-profile subtraction.
- `Spectral Floor`: lower safety floor that helps avoid musical-noise bubbling.
- `Max Reduction`: cap on how far any spectral bin can be attenuated.
- `Smoothing`: temporal smoothing of attenuation changes.
- `Frequency Smoothing`: Audacity-style neighboring-bin gain smoothing that avoids isolated-bin attenuation and reduces robotic/phasey artifacts when several instances run together.
- `Transient Protection`: biases bins that rise clearly above the learned noise profile back toward unity gain so attacks and voiced content do not get mistaken for stationary noise.
- `Mix`: wet/dry blend for conservative parallel cleanup.

Good defaults should favor transparent reduction over maximum silence. Excessive
settings can create chirps, watery artifacts or dull transients. Each reducer
object owns its FFT engine, overlap-add rings, learned profile and gain history;
VST3 processors should therefore instantiate one reducer per audio channel and
per plug-in instance, never a shared/static reducer.

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
