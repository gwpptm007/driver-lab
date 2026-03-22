#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

[[ -d "${PCIUTILS_SRC_DIR}" ]] || die "找不到 pciutils 源码目录：${PCIUTILS_SRC_DIR}"
require_cmd "${CROSS_COMPILE}gcc"

chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" 2>/dev/null || true
chmod +x "${PCIUTILS_SRC_DIR}/configure" 2>/dev/null || true

log "开始构建 guest 侧 arm64 静态 lspci（day24 独立 third_party）"
make -C "${PCIUTILS_SRC_DIR}" clean
make -C "${PCIUTILS_SRC_DIR}" \
    HOST=aarch64-linux-gnu \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    DNS=no ZLIB=no SHARED=no \
    LDFLAGS="-static" \
    lspci

require_file GUEST_LSPCI_BIN "${GUEST_LSPCI_BIN}"
chmod +x "${GUEST_LSPCI_BIN}" || true
file "${GUEST_LSPCI_BIN}" || true
log "guest lspci 已生成：${GUEST_LSPCI_BIN}"
