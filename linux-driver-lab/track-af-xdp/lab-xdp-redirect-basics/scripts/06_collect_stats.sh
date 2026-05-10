#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

REC_DIR="${1:-$(latest_record_dir)}"
OUT="${REC_DIR}/COLLECT_STATS.txt"

{
    write_env_header
    echo
    echo "== record files =="
    find "${REC_DIR}" -maxdepth 1 -type f -printf '%f\n' | sort
    echo
    echo "== xdp attached state =="
    ip -details link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo
    echo "== interface counters =="
    ip -s link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo
    echo "== ethtool stats hint =="
    ethtool -S "${AF_XDP_IFACE}" 2>/dev/null | head -80 || true
    echo
    echo "COLLECT_RESULT=DONE"
} 2>&1 | tee "${OUT}"
