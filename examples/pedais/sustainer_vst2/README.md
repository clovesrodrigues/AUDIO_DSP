# Sustainer VST2

Internal/local VST2 wrapper for `cvdsp::guitar::pedals::SustainerDSP`.

Scope:

- Internal study/build workflow only.
- Initial target: Windows 64-bit VST2 `.dll`.
- No public release or distribution packaging.
- No custom VST2 GUI; use the host's generic parameter editor.

Build from the repository root:

```bash
cmake -S examples/pedais/sustainer_vst2 -B /tmp/sustainer_vst2_build
cmake --build /tmp/sustainer_vst2_build
```

The Windows build writes the plugin DLL to:

```text
/tmp/sustainer_vst2_build/out/vst2/sustainer_vst2.dll
```

