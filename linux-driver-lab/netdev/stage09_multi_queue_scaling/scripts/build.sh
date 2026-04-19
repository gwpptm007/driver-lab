#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# build.sh — 编译 stage09 多队列 netdev 驱动和用户空间工具
# =============================================================================
#
# 【学习要点】
#
# 1. 两阶段编译：driver + tools
#    stage09 与 stage08 不同，没有单独的 modules_install，
#    直接用 make -C KDIR M=PWD 编译驱动
#
# 2. KDIR 变量 — 内核 build 目录
#    KDIR=/lib/modules/$(uname -r)/build
#    指向当前内核的 build 目录，包含 Makefile 和 Module.symvers
#    也可通过环境变量覆盖：KDIR=/path/to/kernel/build
#
# 3. make -C 切换目录编译
#    make -C "$DRIVER_DIR" KDIR="$KDIR" all
#    -C 切换到 DRIVER_DIR 执行 Makefile
#    M=PWD 让内核 makefile 知道模块源码在当前目录
#
# 4. 用户空间工具编译（条件执行）
#    if command -v "$CC_BIN" >/dev/null 2>&1; then ... fi
#    gcc 存在才编译 tools（可能在 minimal 环境中不存在）
#
# 5. 输出归档
#    cp -f "$DRIVER_DIR/netdev_stage09.ko" "$OUT_DIR/"
#    编译产物复制到 output/ 目录，方便后续脚本引用
#
# =============================================================================

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
DRIVER_DIR="$ROOT_DIR/driver"
TOOLS_DIR="$ROOT_DIR/tools"
OUT_DIR="$ROOT_DIR/output"
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
CC_BIN=${CC:-gcc}

mkdir -p "$OUT_DIR"
echo "[stage09] build root: $ROOT_DIR"
echo "[stage09] KDIR: $KDIR"

test -d "$KDIR" || { echo "[stage09] missing kernel build dir: $KDIR" >&2; exit 1; }
make -C "$DRIVER_DIR" KDIR="$KDIR" clean >/dev/null || true
make -C "$DRIVER_DIR" KDIR="$KDIR" all
cp -f "$DRIVER_DIR/netdev_stage09.ko" "$OUT_DIR/"

if command -v "$CC_BIN" >/dev/null 2>&1; then
    make -C "$TOOLS_DIR" CC="$CC_BIN" clean >/dev/null || true
    make -C "$TOOLS_DIR" CC="$CC_BIN" all
fi

echo "[stage09] build done -> $OUT_DIR/netdev_stage09.ko"
