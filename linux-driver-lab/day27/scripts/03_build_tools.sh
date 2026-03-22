#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 构建 guest 侧用户态工具 day27_edu_tool。
# 这一步必须使用 arm64 交叉编译器，因为最终二进制要打进 initramfs 里给 guest 运行。

echo '[day27] 编译 day27 guest 侧用户态工具'
echo "[day27] CC=${CROSS_COMPILE}gcc"
echo "[day27] STRIP=${CROSS_COMPILE}strip"
ensure_dir "${WORKDIR}/tools/aarch64"
"${CROSS_COMPILE}gcc" -O2 -static -Wall -Wextra     -I"${DAY27_ROOT}/include"     -o "${WORKDIR}/tools/aarch64/day27_edu_tool"     "${DAY27_ROOT}/tools/day27_edu_tool.c"
"${CROSS_COMPILE}strip" "${WORKDIR}/tools/aarch64/day27_edu_tool" || true
require_exec "${WORKDIR}/tools/aarch64/day27_edu_tool" day27_edu_tool
echo "[day27] guest 工具已生成：${WORKDIR}/tools/aarch64/day27_edu_tool"
