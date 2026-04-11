#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

rm -rf "${WORKDIR}/rootfs" "${WORKDIR}/rootfs.img" "${WORKDIR}/runs/${RUN_ID}"
log "day23 workdir 临时文件已清理（RUN_ID=${RUN_ID}）"
