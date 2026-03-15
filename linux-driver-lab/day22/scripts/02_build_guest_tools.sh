#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

# day22 现在除了 lspci，还会把一个 guest 侧 C 探针工具打进 rootfs。
# 这个工具不依赖 libpci，只走 sysfs，所以在最小 rootfs 里更稳。

require_cmd "${CC}"
require_cmd "${STRIP}"

log "编译 day22 guest 侧 C 工具"
log "CC=${CC}"
log "STRIP=${STRIP}"

(
    cd "${DAY22_ROOT}/tools"
    make \
        CROSS_COMPILE="${CROSS_COMPILE}" \
        CC="${CC}" \
        STRIP="${STRIP}" \
        OUT_DIR="${TOOLS_DIR}/aarch64"
)

require_executable_file "${TOOLS_DIR}/aarch64/pci_sysfs_dump"
log "guest 工具已生成：${TOOLS_DIR}/aarch64/pci_sysfs_dump"
