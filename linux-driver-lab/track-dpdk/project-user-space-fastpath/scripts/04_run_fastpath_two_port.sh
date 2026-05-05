#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
guard_not_mgmt_pci
require_app_bin

if [[ -z "${DPDK_PCI_1}" ]]; then
    echo "ERROR: DPDK_PCI_1 is empty. Example: sudo DPDK_PCI_1=0000:xx:yy.z $0" >&2
    exit 1
fi

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/FASTPATH_TWO_PORT.log"
CMD_OUT="${RECORD_DIR}/FASTPATH_TWO_PORT_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

APP_ARGS=( $(base_app_args) )
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}_two" -a "${DPDK_PCI}" -a "${DPDK_PCI_1}" -- "${APP_ARGS[@]}")

append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

{
    echo "# FASTPATH_TWO_PORT"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    "${CMD[@]}"
    echo "rc=$?"
} >> "${OUT}" 2>&1

echo "[OK] two-port run saved: ${OUT}"
