#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

ensure_dir "${WORKDIR}/tools/aarch64"

# 这一步只编译 guest 侧用户态工具 day26_edu_tool。
# 它会被打进 initramfs，并在 guest 内被 /init 自动调用。
echo "[day26] 编译 day26 guest 侧用户态工具"
echo "[day26] CC=${CROSS_COMPILE}gcc"
echo "[day26] STRIP=${CROSS_COMPILE}strip"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra \
    -I"${DAY26_ROOT}/include" \
    -o "${WORKDIR}/tools/aarch64/day26_edu_tool" \
    "${DAY26_ROOT}/tools/day26_edu_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day26_edu_tool" || true

require_exec "${WORKDIR}/tools/aarch64/day26_edu_tool"
echo "[day26] guest 工具已生成：${WORKDIR}/tools/aarch64/day26_edu_tool"
