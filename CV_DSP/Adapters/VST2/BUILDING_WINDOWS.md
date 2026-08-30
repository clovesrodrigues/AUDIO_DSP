# Internal VST2 build workflow for Windows 64-bit

This guide is for the internal, local build of the current VST2 examples only.
It does not describe public distribution, installers, signing, or compatibility
with 32-bit hosts.

## Scope and prerequisites

- Target architecture: **Windows 64-bit** only.
- Build toolchain: Visual Studio 2022 (or newer) with the Desktop development
  with C++ workload, including a 64-bit MSVC toolchain.
- Build system: CMake 3.14 or newer, available on `PATH`.
- SDK source: the bundled `backends/VST2_SDK_2.4` directory. Each CMake project
  verifies that `audioeffectx.h` is present before configuration completes.

Open an `x64 Native Tools Command Prompt for Visual Studio`, then run the
commands from the repository root. Using that prompt avoids accidentally
configuring a 32-bit generator/toolchain.

## Build the priority plugins

### Sustainer

```powershell
cmake -S examples/pedais/sustainer_vst2 -B build/sustainer_vst2 -G "Visual Studio 17 2022" -A x64
cmake --build build/sustainer_vst2 --config Release
```

Expected output:

```text
build/sustainer_vst2/out/vst2/Release/sustainer_vst2.dll
```

### Graphic EQ

```powershell
cmake -S examples/graphic_eq_vst2 -B build/graphic_eq_vst2 -G "Visual Studio 17 2022" -A x64
cmake --build build/graphic_eq_vst2 --config Release
```

Expected output:

```text
build/graphic_eq_vst2/out/vst2/Release/graphic_eq_vst2.dll
```

For a single-configuration generator such as Ninja, omit `--config Release`.
The projects place their module in `out/vst2`; the additional `Release`
directory is added by multi-configuration generators such as Visual Studio.

## Internal host validation checklist

1. Copy **one** generated DLL to the VST2 scan directory of an internal
   64-bit VST2-capable host. Do not mix it with a public release location.
2. Trigger a rescan and confirm that the host discovers the plugin.
3. Create a stereo audio track, insert the plugin, and verify that both input
   channels pass audio without clicks during bypass, reload, or sample-rate
   changes.
4. Move every generic-host parameter and confirm the displayed unit/value and
   audible response. For Graphic EQ, verify all ten bands; for Sustainer,
   verify the pedal, gate, and detector controls.
5. Save a host preset/session, reopen it, and confirm that parameter values are
   restored. The adapter stores normalized parameter values in a versioned VST2
   chunk.
6. Repeat steps 2–5 at the sample rates and block sizes used internally.

## Boundaries for later prompts

The current wrappers intentionally have no custom VST2 editor, installer,
signing, 32-bit output, or release packaging. Build and host validation results
should be recorded before adapting any of the remaining examples.
