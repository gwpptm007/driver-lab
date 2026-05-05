#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/L2FWD_TWO_PORT.log"
CMD_OUT="${RECORD_DIR}/L2FWD_TWO_PORT_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

if [[ ! -x "${APP_BIN}" ]]; then
    echo "ERROR: app binary not found: ${APP_BIN}" | tee -a "${OUT}"
    echo "Run: ./scripts/01_build_app.sh" | tee -a "${OUT}"
    exit 1
fi

if [[ -z "${DPDK_PCI_1}" ]]; then
    echo "ERROR: DPDK_PCI_1 is empty. Example:" | tee -a "${OUT}"
    echo "  sudo DPDK_PCI_1=0000:xx:yy.z ./scripts/04_run_l2fwd_two_port.sh" | tee -a "${OUT}"
    exit 1
fi

guard_not_mgmt_pci

cmd=(
    "${APP_BIN}"
    -l "${L2FWD_LCORES}"
    -n "${L2FWD_MEMORY_CHANNELS}"
    --file-prefix "${L2FWD_FILE_PREFIX}"
    -a "${DPDK_PCI}"
    -a "${DPDK_PCI_1}"
    --
    --run-seconds "${L2FWD_RUN_SECONDS}"
    --stats-period "${L2FWD_STATS_PERIOD}"
    --burst-size "${L2FWD_BURST_SIZE}"
    --promisc "${L2FWD_PROMISC}"
)

{
    echo "# L2FWD_TWO_PORT_COMMAND"
    printf '%q ' "${cmd[@]}"
    echo
} > "${CMD_OUT}"
append_command_log "${RECORD_DIR}" "sudo" "${cmd[@]}"

{
    echo "# L2FWD_TWO_PORT"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    echo "## run"
    "${cmd[@]}"
    rc=$?
    echo
    echo "rc=${rc}"
    exit "${rc}"
} >> "${OUT}" 2>&1

echo "[OK] l2fwd two-port log: ${OUT}"
