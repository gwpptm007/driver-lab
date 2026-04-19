#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# build.sh — stage08 驱动和工具的完整构建脚本
# =============================================================================
#
# 【学习要点】
#
# 1. KDIR 的作用
#    内核模块不能独立编译，必须指向目标内核的 build 目录。
#    该目录下有：Module.symvers、体系结构无关的 C 头文件、Makefile 等。
#    构建内核模块的标准路径是 /lib/modules/$(uname -r)/build。
#
# 2. 为什么要 make -C <dir>
#    -C 参数切换到指定目录后再执行 make，等价于 cd <dir> && make。
#    这样做的好处是不需要 cd 到目标目录，脚本可以在任意位置运行。
#
# 3. 外部构建（External Build）模式
#    make -C "$KDIR" M="$PWD" modules
#    KDIR 指向内核源码树（/lib/modules/VERSION/build）
#    M（或者 KBUILD M）指向模块源码目录（当前 driver/）
#    这种方式让内核的 kbuild 系统负责所有编译细节。
#
# 4. 用户态工具的编译
#    tools/ 目录下是普通的 Linux 用户态程序（gcc -O2 -Wall）。
#    它们通过 Makefile 单独构建，不走内核 kbuild 系统。
#    使用 make -C "$TOOLS_DIR" 而非 make -C "$KDIR"，因为它们不需要内核头文件。
#
# 5. 两阶段 clean
#    make clean 在 driver/ 和 tools/ 各执行一次，确保中间文件被清除。
#    内核模块的 .ko 文件是增量编译的，如果只改了一处，最好 clean 后重编。
#
# 6. 输出文件归档
#    最终只保留 .ko（模块文件）和用户态工具，复制到 output/ 目录。
#    output/ 目录是 QEMU 启动时加载模块的参考路径。
#
# =============================================================================

set -euo pipefail

# 【学习】ROOT_DIR 的确定方式
# "$(dirname "$0")" 取得脚本所在目录（scripts/）
# "/.." 退一级得到项目根目录
# && pwd 将相对路径转换为绝对路径
# 这样无论从哪里执行脚本，ROOT_DIR 都能正确指向项目根目录
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
DRIVER_DIR="$ROOT_DIR/driver"
TOOLS_DIR="$ROOT_DIR/tools"
OUT_DIR="$ROOT_DIR/output"

# 【学习】KDIR 环境变量
# 默认使用当前运行内核的 build 目录（uname -r）
# 测试机通常已经安装了对应版本的内核头文件（linux-headers-XXX）
# 如果需要交叉编译（比如为 ARM64 构建），可以覆盖 KDIR 为对应的内核 build 目录
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}

# 【学习】CC 环境变量
# 编译器默认用 gcc，也可以通过 CC=aarch64-linux-gnu-gcc 覆盖
# 驱动用内核的 cc（由 KDIR 中的 Makefile 决定），用户态工具用这里的 CC
CC_BIN=${CC:-gcc}

mkdir -p "$OUT_DIR"

echo "[stage08] build root: $ROOT_DIR"
echo "[stage08] KDIR: $KDIR"

# 【学习】构建前检查
# 检查内核 build 目录是否存在、是否完整
# 典型的 /lib/modules/VERSION/build 实际上是一个符号链接，指向 /usr/src/linux-headers-VERSION
test -d "$KDIR" || { echo "[stage08] missing kernel build dir: $KDIR" >&2; exit 1; }

# =============================================================================
# 阶段 1：构建内核模块
# =============================================================================
# 【学习】make -C + KDIR 是标准的外置内核模块构建方式
# 相当于：cd "$DRIVER_DIR" && make KDIR="$KDIR" all
# KDIR 告诉 kbuild 去哪里找内核头文件、Module.symvers、架构 Makefile
make -C "$DRIVER_DIR" KDIR="$KDIR" clean >/dev/null || true
make -C "$DRIVER_DIR" KDIR="$KDIR" all

# 【学习】.ko 文件的命名
# netdev_stage08.ko 是模块的 ELF 文件，insmod/rmmod 用的就是这个文件名
# 模块被加载后，lsmod 和 /sys/module/netdev_stage08/ 都能看到它
cp -f "$DRIVER_DIR/netdev_stage08.ko" "$OUT_DIR/"

# =============================================================================
# 阶段 2：构建用户态测试工具
# =============================================================================
# 【学习】用户态工具的构建条件
# command -v 检测一个命令是否在 PATH 中且可执行
# 如果没有 gcc，说明测试环境没有开发工具包，跳过用户态工具编译
if command -v "$CC_BIN" >/dev/null 2>&1; then
    make -C "$TOOLS_DIR" CC="$CC_BIN" clean >/dev/null || true
    make -C "$TOOLS_DIR" CC="$CC_BIN" all
else
    echo "[stage08] warning: compiler '$CC_BIN' not found, skip userspace tools" >&2
fi

echo "[stage08] build done -> $OUT_DIR/netdev_stage08.ko"