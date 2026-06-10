# CV Wah-Wah VST3

Foundation VST3 example for `cvdsp::guitar::pedals::WahWahDSP<float>`.

The project follows the existing `examples/pedais` architecture: descriptors are registered through the shared pedal VST3 adapter, parameter state is cached without allocation, CV_GUI is attempted when enabled, and returning `nullptr` from `createView()` preserves the DAW's native/generic editor fallback.

Manual build from the repository root:

```bash
cmake -S examples/pedais/wah_wah_vst3 -B /tmp/wah_wah_vst3_build
cmake --build /tmp/wah_wah_vst3_build --target wah_wah_vst3
```

## Parameter surface

This VST3 forwards the complete manual WahWahDSP descriptor surface: expression pedal, frequency range, dynamic Q range, expression taper, band-pass gain, dry body gain, vintage filter drive, output level, dry/wet mix, input gain and bypass. The `Expr Eng` switch routes the pedal from manual/automation expression to the built-in real-time-safe ExpressionEngine, which analyzes the first input channel plus host BPM/PPQ when available and drives both stereo DSP lanes with a shared generated expression.
