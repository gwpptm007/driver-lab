#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day29] 编译 day29 guest 侧用户态工具'
echo "[day29] CC=${CROSS_COMPILE}gcc"
echo "[day29] STRIP=${CROSS_COMPILE}strip"
ensure_dir "${WORKDIR}/tools/aarch64"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra \
  -I"${DAY29_ROOT}/include" \
  -o "${WORKDIR}/tools/aarch64/day29_edu_dma_tool" \
  "${DAY29_ROOT}/tools/day29_edu_dma_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day29_edu_dma_tool" || true
require_exec "${WORKDIR}/tools/aarch64/day29_edu_dma_tool" day29_edu_dma_tool
echo "[day29] guest 工具已生成：${WORKDIR}/tools/aarch64/day29_edu_dma_tool"
