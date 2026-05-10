#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/CLEAN_RUNTIME.txt"

{
    write_env_header
    echo
    if [[ -x "${APP_DIR}/build/xdp_loader" && -d "/sys/class/net/${AF_XDP_IFACE}" ]]; then
        echo "Detach XDP from ${AF_XDP_IFACE} mode=${AF_XDP_MODE}"
        (cd "${APP_DIR}/build" && ./xdp_loader detach --ifname "${AF_XDP_IFACE}" --mode "${AF_XDP_MODE}") || true
    else
        echo "loader or iface missing, skip detach"
    fi
    echo
    ip -details link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo "CLEAN_RESULT=DONE"
} 2>&1 | tee "${OUT}"
