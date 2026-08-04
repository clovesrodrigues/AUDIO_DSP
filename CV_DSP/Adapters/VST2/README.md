# CV_DSP VST2 adapter

This folder contains a small VST2 adapter skeleton for internal/local builds.

Scope:

- Internal study/build workflow only.
- Initial target: Windows 64-bit VST2 `.dll` wrappers.
- No public release or distribution packaging.
- No custom VST2 GUI in the initial implementation.
- DSP code remains host-independent; VST2-specific code should stay in this
  adapter layer and in `examples/*_vst2` wrappers.

The first intended wrappers are:

1. `examples/pedais/sustainer_vst2`
2. `examples/graphic_eq_vst2`
