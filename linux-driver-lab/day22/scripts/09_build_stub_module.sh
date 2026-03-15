#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

: "${KDIR:?请先 export KDIR=/path/to/kernel/build}"
[[ -d "${KDIR}" ]] || die "KDIR 不存在：${KDIR}"

log "开始构建 day22 PCI stub 模块"
log "KDIR=${KDIR}"

make -C "${DAY22_ROOT}/driver" KDIR="${KDIR}"
log "模块构建完成：${DAY22_ROOT}/driver/day22_ivshmem_stub.ko"
