#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# Day27 需要 guest 侧 lspci 来证明 EDU 设备可见，因此这里单独提供
# 一个“只构建 arm64 静态 lspci”的入口。

echo '[day27] 开始构建 guest 侧 arm64 静态 lspci'
require_file "${PCIUTILS_SRC_DIR}" PCIUTILS_SRC_DIR
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc

chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" 2>/dev/null || true
chmod +x "${PCIUTILS_SRC_DIR}/configure" 2>/dev/null || true

make -C "${PCIUTILS_SRC_DIR}" clean || true
make -C "${PCIUTILS_SRC_DIR}"   HOST=aarch64-linux-gnu   CROSS_COMPILE="${CROSS_COMPILE}"   DNS=no ZLIB=no SHARED=no   LDFLAGS="-static"   lspci

require_exec "${PCIUTILS_SRC_DIR}/lspci" lspci
file "${PCIUTILS_SRC_DIR}/lspci" || true
