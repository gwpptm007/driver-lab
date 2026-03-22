#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

log "开始构建 day24 MMIO 模块"
log "ARCH=${ARCH}"
log "CROSS_COMPILE=${CROSS_COMPILE}"
log "KDIR=${KDIR}"
require_file KDIR "${KDIR}"

make -C "${DAY24_ROOT}/driver" \
    KDIR="${KDIR}" \
    ARCH="${ARCH}" \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    all

require_file MODULE_FILE "${MODULE_FILE}"
log "模块已生成：${MODULE_FILE}"
