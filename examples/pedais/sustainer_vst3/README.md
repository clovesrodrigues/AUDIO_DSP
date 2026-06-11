# CV Sustainer VST3

Foundation VST3 example for `cvdsp::guitar::pedals::SustainerDSP<float>`.

The project follows the existing `examples/pedais` architecture: descriptors are registered through the shared pedal VST3 adapter, parameter state is cached without allocation, CV_GUI is attempted when enabled, and returning `nullptr` from `createView()` preserves the DAW's native/generic editor fallback.

Manual build from the repository root:

```bash
cmake -S examples/pedais/sustainer_vst3 -B /tmp/sustainer_vst3_build
cmake --build /tmp/sustainer_vst3_build --target sustainer_vst3
```

## Parameter surface

This VST3 now exposes and forwards the complete SustainerDSP descriptor surface: sustain amount, envelope attack/release, manual ratio and make-up gain overrides, gate enable/threshold/release, sidechain high-pass, detector mode, max boost, output level, dry/wet mix, input gain and bypass.
