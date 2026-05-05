#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/L2FWD_VDEV_NULL_PAIR.log"
CMD_OUT="${RECORD_DIR}/L2FWD_VDEV_NULL_PAIR_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

if [[ ! -x "${APP_BIN}" ]]; then
    echo "ERROR: app binary not found: ${APP_BIN}" | tee -a "${OUT}"
    echo "Run: ./scripts/01_build_app.sh" | tee -a "${OUT}"
    exit 1
fi

cmd=(
    "${APP_BIN}"
    -l "${L2FWD_LCORES}"
    -n "${L2FWD_MEMORY_CHANNELS}"
    --file-prefix "${L2FWD_FILE_PREFIX}_null"
    --no-pci
    --vdev "${L2FWD_VDEV0}"
    --vdev "${L2FWD_VDEV1}"
    --
    --run-seconds "${L2FWD_RUN_SECONDS}"
    --stats-period "${L2FWD_STATS_PERIOD}"
    --burst-size "${L2FWD_BURST_SIZE}"
    --promisc 0
)

{
    echo "# L2FWD_VDEV_NULL_PAIR_COMMAND"
    printf '%q ' "${cmd[@]}"
    echo
} > "${CMD_OUT}"
append_command_log "${RECORD_DIR}" "sudo" "${cmd[@]}"

{
    echo "# L2FWD_VDEV_NULL_PAIR"
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

echo "[OK] l2fwd vdev null-pair log: ${OUT}"
