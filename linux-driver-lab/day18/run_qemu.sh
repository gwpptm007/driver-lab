#!/usr/bin/env bash
set -euo pipefail

# Day18 run_qemu.sh
# ------------------
# 这份脚本负责“只做启动，不做构建”。
#
# 为什么要单独拆出来？
# 因为 Day18 现在既要支持：
# - build.sh 一步构建后立刻手工启动 QEMU；
# - 你在修改 rootfs / DTB / 内核参数后，单独重复启动；
# - 后续阅读脚本时，清楚知道“构建”和“运行”是两个不同阶段。
#
# 常见用法：
#   ./run_qemu.sh
#   QEMU_MEMORY_MB=2048 ./run_qemu.sh
#   KERNEL_CMDLINE='console=ttyAMA0 root=/dev/ram0 rw rdinit=/init ignore_loglevel' ./run_qemu.sh

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
ROOTFS_IMG="${ROOTFS_IMG:-$SCRIPT_DIR/rootfs.img}"
DTB_IMG="${DTB_IMG:-$SCRIPT_DIR/virt-day18.dtb}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
QEMU_MEMORY_MB="${QEMU_MEMORY_MB:-1024}"
QEMU_CPU="${QEMU_CPU:-cortex-a57}"
MACHINE="${MACHINE:-virt}"
KERNEL_CMDLINE="${KERNEL_CMDLINE:-console=ttyAMA0 root=/dev/ram0 rw rdinit=/init}"

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "command not found: $1"
}

require_cmd "$QEMU_BIN"
[ -d "$KDIR" ] || fail "kernel build dir not found: $KDIR"
[ -f "$KERNEL_IMG" ] || fail "kernel image not found: $KERNEL_IMG"
[ -f "$ROOTFS_IMG" ] || fail "rootfs image not found: $ROOTFS_IMG"
[ -f "$DTB_IMG" ] || fail "dtb image not found: $DTB_IMG"

exec "$QEMU_BIN"     -machine "$MACHINE"     -cpu "$QEMU_CPU"     -m "$QEMU_MEMORY_MB"     -nographic     -kernel "$KERNEL_IMG"     -dtb "$DTB_IMG"     -initrd "$ROOTFS_IMG"     -append "$KERNEL_CMDLINE"
