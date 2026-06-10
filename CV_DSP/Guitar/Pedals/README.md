# CV_DSP Guitar Pedals

This folder contains a header-only, C++20, real-time-safe family of guitar
distortion pedals. The design separates shared infrastructure from concrete
pedal voicings so future pedals can reuse the same parameter IDs, mapping
helpers, gain/mix stages, pre/post filtering and clipping logic.

## Architecture

The common target flow is:

```text
input
→ input gain
→ pre-filter / voice
→ optional oversampling
→ nonlinear clipping
→ DC blocker
→ post-filter / tone
→ output gain
→ dry/wet mix
```

`PedalDriveCore` implements this generic flow for simpler pedals. Pedals that
need special multi-stage processing, such as `ChainsawMetalDSP`, compose the
same smaller building blocks directly.

## Shared headers

- `PedalTypes.hpp`: quality, oversampling, clipping, tone, rectification and
  chainsaw voice enums plus shared numeric ranges.
- `PedalParameterIDs.hpp`: stable neutral IDs for host/adaptor mapping.
- `PedalParameterUtils.hpp`: allocation-free normalized-to-real mapping helpers.
- `PedalGainStage.hpp`: smoothed input/output gain.
- `PedalMix.hpp`: smoothed dry/wet blend with optional phase inversion.
- `PedalClipper.hpp`: parametrized waveshaper with drive, thresholds, bias,
  asymmetry, blend, foldback and multiple clip modes.
- `PedalPreFilter.hpp`: pre-distortion voice EQ using CV_DSP biquads.
- `PedalPostFilter.hpp`: post-distortion tone EQ, fizz cut and notch controls.
- `PedalDriveCore.hpp`: reusable complete drive chain for most pedal designs.
- `../Pedals.hpp`: aggregate include for all pedal infrastructure and concrete
  pedal classes.

## Concrete pedals

### ClassicOverdriveDSP

A mid-forward soft/cubic overdrive. Defaults emphasize a tight pre high-pass,
mid hump and controlled post tone. Important controls include Drive, Tone,
Level, Mix, Pre HPF, Mid Hump, Softness, Bias and Asymmetry.

### VintageHardDistortionDSP

A hard-clipping distortion with a lower pre high-pass and scooped post-EQ. It
adds clip thresholds, threshold link, scoop frequency/Q/amount, high bite and
fizz cut controls.

### VintageFuzzDSP

An asymmetric vintage fuzz with foldback texture, bias, starve, cleanup, input
load, low bloom, rectification and optional gate. It includes an additional
post-rectifier DC blocker because fuzz/rectification can create significant DC.

### ChainsawMetalDSP

A high-gain chainsaw-style distortion with gate, tight low cut, resonant
pre-boost, two clipping stages, interstage gain, dual resonant post-EQ peaks and
fizz cut. Voice modes are Classic Swedish, Modern Tight, Doom Loose and Death
Metal Scoop.

## Parameter descriptors

Each concrete pedal exposes a static `getParameterDescriptors()` table using
`cvdsp::manager::ParameterDescriptor<T>`. These descriptors are immutable,
allocation-free and suitable for future VST3/JUCE/CLAP/iPlug2 adapters.

## Real-time-safety notes

- Processing methods do not allocate, lock or perform I/O.
- Parameter setters may recalculate filter coefficients and should be called on
  control boundaries rather than inside tight per-sample loops when possible.
- Gain, mix, clipper drive and clipper bias are smoothed with existing CV_DSP
  smoothers to reduce zipper noise.
- Nonlinear stages include DC blockers where needed to avoid runaway offset.
- Oversampling Off/2x/4x/8x is available in the shared core or concrete pedals;
  high-gain/fuzz/chainsaw defaults prefer 8x where practical.

## Smoke examples

Standalone smoke examples live under:

- `examples/classic_overdrive_dsp`
- `examples/vintage_hard_distortion_dsp`
- `examples/vintage_fuzz_dsp`
- `examples/chainsaw_metal_dsp`
- `examples/guitar_pedalboard_dsp`

They compile without VST3/GUI dependencies and validate basic processing,
descriptor validity and finite output behavior.

## Checklist for adding a new pedal

1. Reuse `PedalDriveCore` when the standard flow is sufficient.
2. Compose `PedalGainStage`, `PedalMix`, `PedalClipper`, `Biquad`, `DCBlocker`
   and `Oversampling` manually only when topology requires it.
3. Expose musical setters with normalized controls and safe ranges.
4. Add static parameter descriptors using stable IDs.
5. Add a standalone smoke example that checks finite output and descriptor
   validity.
6. Update `Pedals.hpp`, this README and `ROADMAP.md`.
