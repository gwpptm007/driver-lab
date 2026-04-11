#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day32] 编译 day32 guest 侧 perf 工具'
echo "[day32] CC=${CROSS_COMPILE}gcc"
echo "[day32] STRIP=${CROSS_COMPILE}strip"
ensure_dir "${WORKDIR}/tools/aarch64"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra -I"${DAY32_ROOT}/include" -o "${WORKDIR}/tools/aarch64/day32_edu_perf_tool" "${DAY32_ROOT}/tools/day32_edu_perf_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day32_edu_perf_tool" || true
require_exec "${WORKDIR}/tools/aarch64/day32_edu_perf_tool" day32_edu_perf_tool
echo "[day32] guest 工具已生成：${WORKDIR}/tools/aarch64/day32_edu_perf_tool"
