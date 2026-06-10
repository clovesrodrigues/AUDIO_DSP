# CV Phaser VST3

Foundation VST3 example for `cvdsp::guitar::pedals::PhaserDSP<float>`.

The project follows the existing `examples/pedais` architecture: descriptors are registered through the shared pedal VST3 adapter, parameter state is cached without allocation, CV_GUI is attempted when enabled, and returning `nullptr` from `createView()` preserves the DAW's native/generic editor fallback.

Manual build from the repository root:

```bash
cmake -S examples/pedais/phaser_vst3 -B /tmp/phaser_vst3_build
cmake --build /tmp/phaser_vst3_build --target phaser_vst3
```

## Parameter surface

This VST3 forwards the complete PhaserDSP descriptor surface: modulation rate/depth, feedback, stage count, sweep minimum/maximum frequencies, LFO waveform, output level, dry/wet mix, input gain and bypass.
