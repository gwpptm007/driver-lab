#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 编译 day25 用户态工具。它会被放进 guest rootfs 中，
# 用于读取驱动信息、触发 EDU 中断、读取 count/status。
ensure_dir "${WORKDIR}/tools/aarch64"
ensure_dir "${WORKDIR}/tools/host"

echo "[day25] 编译 day25 guest 侧用户态工具"
echo "[day25] CC=${CROSS_COMPILE}gcc"
echo "[day25] STRIP=${CROSS_COMPILE}strip"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra     -I"${DAY25_ROOT}/include"     -o "${WORKDIR}/tools/aarch64/day25_irq_tool"     "${DAY25_ROOT}/tools/day25_irq_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day25_irq_tool" || true

require_exec "${WORKDIR}/tools/aarch64/day25_irq_tool"
echo "[day25] guest 工具已生成：${WORKDIR}/tools/aarch64/day25_irq_tool"
