#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${1:-/tmp/audio_dsp_new_dsp_smokes}"
compiler="${CXX:-c++}"
common_flags=(-std=c++20 -Wall -Wextra -Wpedantic -I"${repo_root}")

mkdir -p "${build_root}"

compile_and_run() {
    local name="$1"
    local source="$2"
    local output="${build_root}/${name}"
    echo "[build] ${name}"
    "${compiler}" "${common_flags[@]}" "${repo_root}/${source}" -o "${output}"
    echo "[run]   ${name}"
    "${output}"
}

compile_and_run expression_engine_smoke examples/expression_engine_dsp/expression_engine_smoke.cpp
compile_and_run phaser_smoke examples/phaser_dsp/phaser_smoke.cpp
compile_and_run sustainer_smoke examples/sustainer_dsp/sustainer_smoke.cpp
compile_and_run wah_wah_smoke examples/wah_wah_dsp/wah_wah_smoke.cpp
compile_and_run guitar_pedalboard_smoke examples/guitar_pedalboard_dsp/guitar_pedalboard_smoke.cpp

spectral_build="${build_root}/spectral_noise_reducer_dsp"
echo "[cmake] spectral_noise_reducer_dsp"
cmake -S "${repo_root}/examples/spectral_noise_reducer_dsp" -B "${spectral_build}"
cmake --build "${spectral_build}" -j2

echo "[run]   spectral_noise_reducer_smoke"
"${spectral_build}/spectral_noise_reducer_smoke"
echo "[run]   realtime_noise_reducer_benchmark"
"${spectral_build}/realtime_noise_reducer_benchmark"

echo "All new DSP smoke examples and benchmarks passed."
