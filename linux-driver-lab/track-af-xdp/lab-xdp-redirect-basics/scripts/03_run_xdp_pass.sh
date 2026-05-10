#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/XDP_PASS.log"
CMD="${REC_DIR}/XDP_PASS_COMMAND.txt"

if [[ ! -x "${APP_DIR}/build/xdp_loader" ]]; then
    echo "ERROR: xdp_loader not found. Run ./scripts/01_build_app.sh first." >&2
    exit 1
fi

cat > "${CMD}" <<EOF2
sudo AF_XDP_IFACE=${AF_XDP_IFACE} AF_XDP_MODE=${AF_XDP_MODE} AF_XDP_DURATION=${AF_XDP_DURATION} AF_XDP_INTERVAL=${AF_XDP_INTERVAL} ./scripts/03_run_xdp_pass.sh
EOF2
run_loader "pass" "${AF_XDP_DURATION}" "${OUT}"
