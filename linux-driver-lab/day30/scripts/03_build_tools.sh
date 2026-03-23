#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day30] 编译 day30 guest 侧用户态工具'
echo "[day30] CC=${CROSS_COMPILE}gcc"
echo "[day30] STRIP=${CROSS_COMPILE}strip"
ensure_dir "${WORKDIR}/tools/aarch64"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra \
  -I"${DAY30_ROOT}/include" \
  -o "${WORKDIR}/tools/aarch64/day30_edu_mmap_tool" \
  "${DAY30_ROOT}/tools/day30_edu_mmap_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day30_edu_mmap_tool" || true
require_exec "${WORKDIR}/tools/aarch64/day30_edu_mmap_tool" day30_edu_mmap_tool
echo "[day30] guest 工具已生成：${WORKDIR}/tools/aarch64/day30_edu_mmap_tool"
