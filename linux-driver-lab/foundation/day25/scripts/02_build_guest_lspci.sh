#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 独立构建 guest 侧 arm64 静态 lspci。
# day25 不直接内置第三方源码，因此主流程要求用户先 git clone pciutils，
# 这里再统一做 configure 权限修复和交叉编译。
require_file "${PCIUTILS_SRC_DIR:-}" PCIUTILS_SRC_DIR
if [ ! -d "${PCIUTILS_SRC_DIR}" ]; then
    echo "[day25][ERROR] 找不到 pciutils 源码目录：${PCIUTILS_SRC_DIR}" >&2
    exit 1
fi

if [ -f "${PCIUTILS_SRC_DIR}/lib/configure" ]; then
    chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" || true
fi
if [ -f "${PCIUTILS_SRC_DIR}/configure" ]; then
    chmod +x "${PCIUTILS_SRC_DIR}/configure" || true
fi

echo "[day25] 开始构建 guest 侧 arm64 静态 lspci"
make -C "${PCIUTILS_SRC_DIR}" clean || true
make -C "${PCIUTILS_SRC_DIR}"     HOST=aarch64-linux-gnu     CROSS_COMPILE="${CROSS_COMPILE}"     DNS=no ZLIB=no SHARED=no     LDFLAGS="-static"     lspci

require_exec "${PCIUTILS_SRC_DIR}/lspci" "${PCIUTILS_SRC_DIR}/lspci"
echo "[day25] arm64 静态 lspci 已生成：${PCIUTILS_SRC_DIR}/lspci"
