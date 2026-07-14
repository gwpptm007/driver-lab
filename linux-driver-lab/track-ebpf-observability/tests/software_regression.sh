#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
skip_clean="${SKIP_CLEAN:-0}"

"${root}/tests/check_fundamentals.sh"
python3 "${root}/tests/check_visual_assets.py"

for project in lab-libbpf-net-observer project-linux-network-observability; do
  echo "EBPF_BUILD_BEGIN target=${project}"
  if [[ "${skip_clean}" != "1" ]]; then
    make -C "${root}/${project}" clean
  fi
  make -C "${root}/${project}"
  echo "EBPF_BUILD_PASS target=${project}"
done

echo "EBPF_OBSERVABILITY_FUNDAMENTALS_VISUALS_AND_BUILD_PASS"
