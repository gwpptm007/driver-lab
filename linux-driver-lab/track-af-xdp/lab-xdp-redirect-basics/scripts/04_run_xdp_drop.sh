#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root
refuse_management_iface "XDP_DROP"

if [[ "${AF_XDP_CONFIRM_DROP:-NO}" != "YES" ]]; then
    echo "ERROR: XDP_DROP can disrupt traffic on ${AF_XDP_IFACE}." >&2
    echo "Run with: sudo AF_XDP_CONFIRM_DROP=YES $0" >&2
    exit 2
fi

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/XDP_DROP.log"
CMD="${REC_DIR}/XDP_DROP_COMMAND.txt"

if [[ ! -x "${APP_DIR}/build/xdp_loader" ]]; then
    echo "ERROR: xdp_loader not found. Run ./scripts/01_build_app.sh first." >&2
    exit 1
fi

cat > "${CMD}" <<EOF2
sudo AF_XDP_CONFIRM_DROP=YES AF_XDP_IFACE=${AF_XDP_IFACE} AF_XDP_MODE=${AF_XDP_MODE} AF_XDP_DURATION=${AF_XDP_DURATION} AF_XDP_INTERVAL=${AF_XDP_INTERVAL} ./scripts/04_run_xdp_drop.sh
EOF2
run_loader "drop" "${AF_XDP_DURATION}" "${OUT}"
