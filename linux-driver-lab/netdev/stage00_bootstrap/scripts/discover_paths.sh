#!/usr/bin/env bash
set -euo pipefail
mkdir -p output workdir
OUT=output/discovered_paths.env
: > "$OUT"

echo "TARGET_ARCH=${TARGET_ARCH:-host}" >> "$OUT"
echo "RUN_MODE=${RUN_MODE:-host}" >> "$OUT"
echo "QEMU_AARCH64=$(command -v qemu-system-aarch64 || true)" >> "$OUT"
echo "QEMU_X86_64=$(command -v qemu-system-x86_64 || true)" >> "$OUT"
echo "CC_HOST=$(command -v ${HOST_CC:-gcc} || true)" >> "$OUT"
echo "CROSS_AARCH64=$(command -v aarch64-linux-gnu-gcc || true)" >> "$OUT"
echo "KERNEL_IMAGE_HINT=${KERNEL_IMAGE:-}" >> "$OUT"
echo "ROOTFS_IMAGE_HINT=${ROOTFS_IMAGE:-}" >> "$OUT"

echo "[stage00] discovered paths written to $OUT"
