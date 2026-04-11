#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 清理 workdir 和构建产物，便于重新从干净状态验证 Day27。
rm -rf "${WORKDIR}"
make -C "${DAY27_ROOT}/driver" KDIR="${KDIR}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" clean >/dev/null 2>&1 || true
rm -f "${DAY27_ROOT}/records/${RUN_ID}"/*.txt "${DAY27_ROOT}/records/${RUN_ID}"/*.log "${DAY27_ROOT}/records/${RUN_ID}"/*.md 2>/dev/null || true
echo '[day27] 已清理 workdir 与当前 run 产物'
