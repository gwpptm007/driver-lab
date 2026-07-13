#!/usr/bin/env bash
set -euo pipefail

track_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
skip_clean="${SKIP_CLEAN:-0}"

"${track_root}/tests/check_fundamentals.sh"

for project in \
  lab-xdp-redirect-basics \
  lab-af-xdp-socket-rings \
  lab-af-xdp-zero-copy-vs-copy \
  project-af-xdp-mini-forwarder; do
  app_dir="${track_root}/${project}/app"
  echo "AF_XDP_BUILD_BEGIN target=${project}"
  if [[ "${skip_clean}" != "1" ]]; then
    make -C "${app_dir}" clean
  fi
  make -C "${app_dir}"
  echo "AF_XDP_BUILD_PASS target=${project}"
done

echo "AF_XDP_FUNDAMENTALS_AND_BUILD_REGRESSION_PASS"

