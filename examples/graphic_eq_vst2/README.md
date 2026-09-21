# Graphic EQ VST2

Internal/local VST2 wrapper for the 10-band `cvdsp::eq::GraphicEQ` DSP.

Scope:

- Internal study/build workflow only.
- Initial target: Windows 64-bit VST2 `.dll`.
- No public release or distribution packaging.
- No custom VST2 GUI; use the host's generic parameter editor.
- Ten automatable bands from 31 Hz through 16 kHz, each covering -24 dB to +24 dB.

Build from the repository root:

```bash
cmake -S examples/graphic_eq_vst2 -B /tmp/graphic_eq_vst2_build
cmake --build /tmp/graphic_eq_vst2_build
```

The Windows build writes the plugin DLL to:

```text
/tmp/graphic_eq_vst2_build/out/vst2/graphic_eq_vst2.dll
```
