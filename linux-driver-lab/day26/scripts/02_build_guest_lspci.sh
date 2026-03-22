#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 只在当前 day26 目录内构建第三方 lspci，不复用其他 day 的产物。
require_file "${PCIUTILS_SRC_DIR:-}" PCIUTILS_SRC_DIR
if [ ! -d "${PCIUTILS_SRC_DIR}" ]; then
    echo "[day26][ERROR] 找不到 pciutils 源码目录：${PCIUTILS_SRC_DIR}" >&2
    exit 1
fi

# zip 解压后 configure 可能丢执行位，这里自动补一次。
if [ -f "${PCIUTILS_SRC_DIR}/lib/configure" ]; then
    chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" || true
fi
if [ -f "${PCIUTILS_SRC_DIR}/configure" ]; then
    chmod +x "${PCIUTILS_SRC_DIR}/configure" || true
fi

echo "[day26] 开始构建 guest 侧 arm64 静态 lspci"
make -C "${PCIUTILS_SRC_DIR}" clean || true
make -C "${PCIUTILS_SRC_DIR}" \
    HOST=aarch64-linux-gnu \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    DNS=no ZLIB=no SHARED=no \
    LDFLAGS="-static" \
    lspci

require_exec "${PCIUTILS_SRC_DIR}/lspci" "${PCIUTILS_SRC_DIR}/lspci"
echo "[day26] arm64 静态 lspci 已生成：${PCIUTILS_SRC_DIR}/lspci"
