#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
safe_socket_path_check

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/CLEAN_RUNTIME.txt"
: > "${OUT}"

{
    echo "# CLEAN_RUNTIME"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Before"
    ls -l "${VHOST_SOCKET}" 2>/dev/null || true
    ps -ef | grep -E "dpdk-testpmd|testpmd|${TESTPMD_FILE_PREFIX}" | grep -v grep || true
} >> "${OUT}" 2>&1

# Do not blindly kill. Only remove stale socket if no matching testpmd remains.
if ps -ef | grep -E "dpdk-testpmd|testpmd|${TESTPMD_FILE_PREFIX}" | grep -v grep >/dev/null 2>&1; then
    echo "WARN: testpmd-like process still exists; not removing socket." >> "${OUT}"
else
    rm -f "${VHOST_SOCKET}"
fi

{
    echo
    echo "## After"
    ls -l "${VHOST_SOCKET}" 2>/dev/null || true
    ps -ef | grep -E "dpdk-testpmd|testpmd|${TESTPMD_FILE_PREFIX}" | grep -v grep || true
} >> "${OUT}" 2>&1

cat <<EOF_OUT
[OK] Clean runtime saved:
${OUT}
EOF_OUT
