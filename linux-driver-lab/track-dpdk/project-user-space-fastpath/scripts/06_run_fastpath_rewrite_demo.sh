#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
require_app_bin

# Use vdev null pair by default so the rewrite code path can be smoke-tested
# without touching the physical NIC.  To run against VMXNET3, copy the printed
# app args and use 03_run_fastpath_single_port.sh / 04_run_fastpath_two_port.sh.
FASTPATH_UDP_ONLY=1
FASTPATH_REWRITE_ENABLE=1
FASTPATH_EXTRA_APP_ARGS="--rewrite-src-ip 10.10.1.10 --rewrite-dst-ip 10.10.2.20 --rewrite-src-port 5000 --rewrite-dst-port 6000 ${FASTPATH_EXTRA_APP_ARGS}"
export FASTPATH_UDP_ONLY FASTPATH_REWRITE_ENABLE FASTPATH_EXTRA_APP_ARGS

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/FASTPATH_REWRITE_DEMO.log"
CMD_OUT="${RECORD_DIR}/FASTPATH_REWRITE_DEMO_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

APP_ARGS=( $(base_app_args) )
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}_rewrite" --no-pci --vdev "${FASTPATH_VDEV0}" --vdev "${FASTPATH_VDEV1}" -- "${APP_ARGS[@]}")

append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

{
    echo "# FASTPATH_REWRITE_DEMO"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    "${CMD[@]}"
    echo "rc=$?"
} >> "${OUT}" 2>&1

echo "[OK] rewrite demo saved: ${OUT}"
