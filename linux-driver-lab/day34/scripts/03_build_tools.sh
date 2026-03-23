#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day34] 编译 day34 guest 侧稳定性用户态工具'
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
require_exec "${CROSS_COMPILE}strip" CROSS_COMPILE-strip
ensure_dir "${WORKDIR}/tools/aarch64"

echo "[day34] CC=${CROSS_COMPILE}gcc"
echo "[day34] STRIP=${CROSS_COMPILE}strip"
"${CROSS_COMPILE}gcc" -O2 -Wall -Wextra -static   -I"${DAY34_ROOT}/include"   -o "${WORKDIR}/tools/aarch64/day34_edu_stability_tool"   "${DAY34_ROOT}/tools/day34_edu_stability_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day34_edu_stability_tool" || true
echo "[day34] guest 工具已生成：${WORKDIR}/tools/aarch64/day34_edu_stability_tool"
