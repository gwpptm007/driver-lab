#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

# day22 现在除了 lspci，还会把一个 guest 侧 C 探针工具打进 rootfs。
# 这个工具不依赖 libpci，只走 sysfs，所以在最小 rootfs 里更稳。
#
# 注意：
#   某些情况下，用户之前跑过旧版本 day22、手工清理过 workdir，或者 zip 解压/覆盖后留下了异常状态，
#   会出现 tools/Makefile 判断 “Nothing to be done for all”，但最终产物实际上不存在或没有执行位。
#   因此这里采用“先尝试正常构建；若产物异常则强制重建”的方式，把问题收口在脚本里。

require_cmd "${CC}"
require_cmd "${STRIP}"

TARGET="${TOOLS_DIR}/aarch64/pci_sysfs_dump"

log "编译 day22 guest 侧 C 工具"
log "CC=${CC}"
log "STRIP=${STRIP}"

build_once() {
    (
        cd "${DAY22_ROOT}/tools"
        make             CROSS_COMPILE="${CROSS_COMPILE}"             CC="${CC}"             STRIP="${STRIP}"             OUT_DIR="${TOOLS_DIR}/aarch64"             "$@"
    )
}

build_once

if [[ -e "${TARGET}" && ! -x "${TARGET}" ]]; then
    warn "检测到产物存在但不可执行，尝试补执行位：${TARGET}"
    chmod +x "${TARGET}" || true
fi

if [[ ! -x "${TARGET}" ]]; then
    warn "首次构建后产物仍异常，执行强制重建：${TARGET}"
    rm -f "${TARGET}"
    build_once -B
fi

if [[ -e "${TARGET}" && ! -x "${TARGET}" ]]; then
    warn "强制重建后产物仍不可执行，尝试再次补执行位：${TARGET}"
    chmod +x "${TARGET}" || true
fi

require_executable_file "${TARGET}"
log "guest 工具已生成：${TARGET}"
