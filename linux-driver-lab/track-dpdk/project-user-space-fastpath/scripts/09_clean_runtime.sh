#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/CLEAN_RUNTIME.txt"
: > "${OUT}"
append_command_log "${RECORD_DIR}" "$0"

{
    echo "# CLEAN_RUNTIME"
    echo
    echo "## kill stale fastpath-lite"
    pkill -f fastpath-lite 2>/dev/null || true
    echo
    echo "## remove DPDK runtime leftovers"
    rm -rf /var/run/dpdk/fastpath_lite* /var/run/dpdk/rte/fastpath_lite* 2>/dev/null || true
    rm -rf /tmp/dpdk/fastpath_lite* 2>/dev/null || true
    echo "done"
} >> "${OUT}" 2>&1

echo "[OK] clean saved: ${OUT}"
