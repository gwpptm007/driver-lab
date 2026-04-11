#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

: "${KDIR:?请先 export KDIR=/path/to/kernel/build}"
[[ -d "${KDIR}" ]] || die "KDIR 不存在：${KDIR}"

log "开始构建 day22 PCI stub 模块"
log "KDIR=${KDIR}"
log "ARCH=${ARCH}"
log "CROSS_COMPILE=${CROSS_COMPILE}"

make -C "${DAY22_ROOT}/driver" KDIR="${KDIR}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}"
log "模块构建完成：${DAY22_ROOT}/driver/day22_ivshmem_stub.ko"
