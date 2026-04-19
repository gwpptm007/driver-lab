#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# build.sh — 编译 stage10 MSI-X per-queue 驱动和用户空间工具

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
DRIVER_DIR="$ROOT_DIR/driver"
TOOLS_DIR="$ROOT_DIR/tools"
OUT_DIR="$ROOT_DIR/output"
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
CC_BIN=${CC:-gcc}

mkdir -p "$OUT_DIR"
echo "[stage10] build root: $ROOT_DIR"
echo "[stage10] KDIR: $KDIR"

test -d "$KDIR" || { echo "[stage10] missing kernel build dir: $KDIR" >&2; exit 1; }
make -C "$DRIVER_DIR" KDIR="$KDIR" clean >/dev/null || true
make -C "$DRIVER_DIR" KDIR="$KDIR" all
cp -f "$DRIVER_DIR/netdev_stage10.ko" "$OUT_DIR/"

if command -v "$CC_BIN" >/dev/null 2>&1; then
    make -C "$TOOLS_DIR" CC="$CC_BIN" clean >/dev/null || true
    make -C "$TOOLS_DIR" CC="$CC_BIN" all
fi

echo "[stage10] build done -> $OUT_DIR/netdev_stage10.ko"
