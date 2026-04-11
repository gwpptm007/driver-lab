#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# day31 工具必须静态链接，才能直接放进 initramfs 使用，
# 避免 guest 内再准备额外共享库。
echo '[day31] 编译 day31 guest 侧 bench 工具'
echo "[day31] CC=${CROSS_COMPILE}gcc"
echo "[day31] STRIP=${CROSS_COMPILE}strip"
ensure_dir "${WORKDIR}/tools/aarch64"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra   -I"${DAY31_ROOT}/include"   -o "${WORKDIR}/tools/aarch64/day31_edu_bench_tool"   "${DAY31_ROOT}/tools/day31_edu_bench_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day31_edu_bench_tool" || true
require_exec "${WORKDIR}/tools/aarch64/day31_edu_bench_tool" day31_edu_bench_tool
echo "[day31] guest 工具已生成：${WORKDIR}/tools/aarch64/day31_edu_bench_tool"
