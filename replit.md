# AUDIO_DSP

A high-performance C++20 real-time digital signal processing library for building audio plugins (VST3), virtual instruments, guitar pedals, and spectral analysis tools.

## Project Overview

- **Language:** C++20
- **Build System:** CMake 3.14+
- **Plugin Format:** VST3 (Steinberg SDK bundled)
- **GUI Layer:** Dear ImGui + OpenGL 3 (optional, `CV_GUI/`)
- **Dependencies:** All bundled in `backends/` (no package manager needed)

## Structure

- `CV_DSP/` — Core header-only DSP library (filters, dynamics, reverb, modulation, spectral, guitar, etc.)
- `CV_GUI/` — Optional ImGui-based GUI layer for plugins
- `CV_OBS_PLUGIN/` — OBS Studio audio filter plugin
- `examples/` — Pure DSP smoke tests and full VST3 plugin examples
- `backends/` — Bundled third-party SDKs (VST3, ImGui, libobs, TinySoundFont)

## Building

### Pure DSP smoke tests (no DAW needed)

```bash
# Run all DSP smoke tests at once
bash examples/run_new_dsp_smokes.sh

# Or compile a single smoke test manually
g++ -std=c++20 -O2 -I. examples/chainsaw_metal_dsp/chainsaw_metal_smoke.cpp -o /tmp/smoke && /tmp/smoke
```

### VST3 plugins (requires CMake)

```bash
cd examples/chorus_vst3
cmake -B build
cmake --build build -j$(nproc)
```

## User Preferences

- Documentation language: Portuguese (README and docs are in PT-BR)
