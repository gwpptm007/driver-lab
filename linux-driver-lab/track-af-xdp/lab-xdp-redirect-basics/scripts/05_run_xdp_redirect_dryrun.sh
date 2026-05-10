#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root
refuse_management_iface "XDP_REDIRECT dry-run"

if [[ "${AF_XDP_CONFIRM_REDIRECT:-NO}" != "YES" ]]; then
    echo "ERROR: redirect dry-run may drop packets when XSKMAP has no AF_XDP socket entry." >&2
    echo "Run with: sudo AF_XDP_CONFIRM_REDIRECT=YES $0" >&2
    exit 2
fi

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/XDP_REDIRECT_DRYRUN.log"
CMD="${REC_DIR}/XDP_REDIRECT_DRYRUN_COMMAND.txt"
DURATION="${AF_XDP_REDIRECT_DURATION:-3}"

if [[ ! -x "${APP_DIR}/build/xdp_loader" ]]; then
    echo "ERROR: xdp_loader not found. Run ./scripts/01_build_app.sh first." >&2
    exit 1
fi

cat > "${CMD}" <<EOF2
sudo AF_XDP_CONFIRM_REDIRECT=YES AF_XDP_IFACE=${AF_XDP_IFACE} AF_XDP_MODE=${AF_XDP_MODE} AF_XDP_REDIRECT_DURATION=${DURATION} ./scripts/05_run_xdp_redirect_dryrun.sh
EOF2
run_loader "redirect" "${DURATION}" "${OUT}"
