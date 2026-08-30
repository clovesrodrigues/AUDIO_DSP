# CV_DSP VST2 adapter

This folder contains a small VST2 adapter layer for internal/local builds.

Scope:

- Internal study/build workflow only.
- Initial target: Windows 64-bit VST2 `.dll` wrappers.
- No public release or distribution packaging.
- No custom VST2 GUI in the initial implementation.
- DSP code remains host-independent; VST2-specific code should stay in this
  adapter layer and in `examples/*_vst2` wrappers.

The adapter provides the shared host-facing mechanics: normalized parameter
storage, descriptor formatting, VST2 automation queries, state chunks, the
`AudioEffectX` lifecycle, and the entry-point helper. A wrapper owns only its
DSP instance(s), descriptors, parameter-to-DSP mapping, and audio processing.

The first intended wrappers are:

1. `examples/pedais/sustainer_vst2`
2. `examples/graphic_eq_vst2`

## Windows build and validation

Use the Windows 64-bit workflow in [BUILDING_WINDOWS.md](BUILDING_WINDOWS.md)
to configure either example, find the resulting `.dll`, and perform the
internal host validation. The CMake projects intentionally use the repository's
bundled VST2 SDK and do not package files for distribution.
