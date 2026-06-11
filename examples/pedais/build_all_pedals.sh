#!/usr/bin/env bash
set -euo pipefail

# Build all CV_DSP guitar pedal VST3 examples in isolated build directories.
# Usage:
#   examples/pedais/build_all_pedals.sh [build-root]
#
# The default build root intentionally lives outside the repository so local
# builds do not pollute the source tree.

readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly repo_root="$(cd -- "${script_dir}/../.." && pwd)"
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<USAGE
Usage: examples/pedais/build_all_pedals.sh [build-root]

Builds only guitar pedal VST3 examples under examples/pedais.
Non-pedal utilities, such as examples/spectral_noise_reducer_vst3, are built separately.

Default build root:
  /tmp/cv_dsp_pedais_vst3_build
USAGE
  exit 0
fi

readonly build_root="${1:-/tmp/cv_dsp_pedais_vst3_build}"

readonly pedals=(
  "classic_overdrive_vst3"
  "vintage_hard_distortion_vst3"
  "vintage_fuzz_vst3"
  "chainsaw_metal_vst3"
  "sustainer_vst3"
  "phaser_vst3"
  "wah_wah_vst3"
)

mkdir -p "${build_root}"

for pedal in "${pedals[@]}"; do
  echo "==> Configuring ${pedal}"
  cmake -S "${repo_root}/examples/pedais/${pedal}" -B "${build_root}/${pedal}"

  echo "==> Building ${pedal}"
  cmake --build "${build_root}/${pedal}" --target "${pedal}" --parallel

done

cat <<SUMMARY

All ${#pedals[@]} pedal VST3 examples built successfully.
Build root: ${build_root}

Linux .vst3 outputs are under:
  ${build_root}/<pedal>/VST3/<pedal>.vst3/Contents/x86_64-linux/
SUMMARY
