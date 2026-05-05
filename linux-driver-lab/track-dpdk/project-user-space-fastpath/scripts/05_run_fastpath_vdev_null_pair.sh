#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
require_app_bin

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/FASTPATH_VDEV_NULL_PAIR.log"
CMD_OUT="${RECORD_DIR}/FASTPATH_VDEV_NULL_PAIR_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

APP_ARGS=( $(base_app_args) )
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}_null" --no-pci --vdev "${FASTPATH_VDEV0}" --vdev "${FASTPATH_VDEV1}" -- "${APP_ARGS[@]}")

append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

{
    echo "# FASTPATH_VDEV_NULL_PAIR"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    "${CMD[@]}"
    echo "rc=$?"
} >> "${OUT}" 2>&1

echo "[OK] vdev null pair run saved: ${OUT}"
