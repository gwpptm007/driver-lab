#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day33] 编译 day33 guest 侧 trace 用户态工具'
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
require_exec "${CROSS_COMPILE}strip" CROSS_COMPILE-strip
ensure_dir "${WORKDIR}/tools/aarch64"

echo "[day33] CC=${CROSS_COMPILE}gcc"
echo "[day33] STRIP=${CROSS_COMPILE}strip"
"${CROSS_COMPILE}gcc" -O2 -Wall -Wextra -static   -I"${DAY33_ROOT}/include"   -o "${WORKDIR}/tools/aarch64/day33_edu_trace_tool"   "${DAY33_ROOT}/tools/day33_edu_trace_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day33_edu_trace_tool" || true
echo "[day33] guest 工具已生成：${WORKDIR}/tools/aarch64/day33_edu_trace_tool"
