#!/usr/bin/env bash
# ================================================================================
# stage07 build.sh — stage07_real_queue_model 编译脚本
#
# 【功能】
# 1. 编译 kernel 模块 driver/netdev_stage07.c → output/netdev_stage07.ko
# 2. 编译 userspace 工具 tools/send_stage07_frame 和 recv_stage07_frame
#
# 【环境变量】
# - KDIR  : Kernel build directory（默认 /lib/modules/$(uname -r)/build）
# - CC     : C 编译器（默认 gcc，可改为 aarch64-linux-gnu-gcc 做 ARM64 交叉编译）
#
# 【使用示例】
#   # 在本机编译（默认）
#   ./scripts/build.sh
#
#   # 指定 kernel build 目录
#   KDIR=/path/to/kernel/build ./scripts/build.sh
#
#   # ARM64 交叉编译（需要 aarch64-linux-gnu-gcc）
#   KDIR=/path/to/arm64/kernel/build CC=aarch64-linux-gnu-gcc ./scripts/build.sh
#
# 【输出】
# - output/netdev_stage07.ko    — kernel 模块
# - output/send_stage07_frame   — 发送工具（如果编译成功）
# - output/recv_stage07_frame   — 接收工具（如果编译成功）
# ================================================================================

set -euo pipefail

# ROOT_DIR: stage07_real_queue_model 根目录
# $(dirname "$0")/.. 会解析到 stage07_real_queue_model/
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
DRIVER_DIR="$ROOT_DIR/driver"
TOOLS_DIR="$ROOT_DIR/tools"
OUT_DIR="$ROOT_DIR/output"

# KDIR: Kernel build directory
# 构建 kernel 模块需要指向内核源码编译产物（包含 Module.symvers / .config 等）
# 默认取当前运行内核的 build 目录（uname -r）
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}

# CC_BIN: C 编译器
# - gcc        : 本机编译（x86_64）
# - aarch64-linux-gnu-gcc : ARM64 交叉编译
CC_BIN=${CC:-gcc}

mkdir -p "$OUT_DIR"

echo "[stage07] build root: $ROOT_DIR"
echo "[stage07] KDIR: $KDIR"

# 1. 检查 kernel build 目录是否存在
#    如果 KDIR 指向的目录没有 Module.symvers，MODPOST 会失败
test -d "$KDIR" || { echo "[stage07] missing kernel build dir: $KDIR" >&2; exit 1; }

# 2. 编译 kernel 模块
#    make -C KDIR M=PWD modules:
#    - -C KDIR      : 切换到 kernel build 目录执行 make
#    - M=PWD        : 告诉 kernel make system 当前目录是外部模块源码
#    - modules      : 执行 kernel build system 的 modules 目标
#    产物: driver/netdev_stage07.ko
make -C "$DRIVER_DIR" KDIR="$KDIR" clean >/dev/null
make -C "$DRIVER_DIR" KDIR="$KDIR" all

# 3. 复制模块到 output 目录
cp -f "$DRIVER_DIR/netdev_stage07.ko" "$OUT_DIR/"

# 4. 编译 userspace 工具（send/recv）
#    userspace 工具需要用 host gcc 编译（不是 kernel gcc）
#    如果指定了 CC=aarch64-linux-gnu-gcc，userspace 工具也会用 cross compiler
if command -v "$CC_BIN" >/dev/null 2>&1; then
    make -C "$TOOLS_DIR" CC="$CC_BIN" clean >/dev/null || true
    make -C "$TOOLS_DIR" CC="$CC_BIN" all
else
    echo "[stage07] warning: compiler '$CC_BIN' not found, skip userspace tools" >&2
fi

echo "[stage07] build done -> $OUT_DIR/netdev_stage07.ko"
