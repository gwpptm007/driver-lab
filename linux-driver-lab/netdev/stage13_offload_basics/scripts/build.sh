#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# build.sh — 编译 stage12_soft 驱动和用户空间工具

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
STAGE13_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
DRIVER_DIR="$STAGE13_DIR/driver"
TOOLS_DIR="$STAGE13_DIR/tools"
OUT_DIR="$STAGE13_DIR/output"
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
CC_BIN=${CC:-gcc}

mkdir -p "$OUT_DIR"
echo "[stage13_soft] build root: $STAGE13_DIR"
echo "[stage13_soft] KDIR: $KDIR"

test -d "$KDIR" || { echo "[stage13_soft] missing kernel build dir: $KDIR" >&2; exit 1; }
make -C "$DRIVER_DIR" KDIR="$KDIR" clean >/dev/null || true
make -C "$DRIVER_DIR" KDIR="$KDIR" all
cp -f "$DRIVER_DIR/netdev_stage13_soft.ko" "$OUT_DIR/"

if command -v "$CC_BIN" >/dev/null 2>&1; then
    make -C "$TOOLS_DIR" CC="$CC_BIN" clean >/dev/null || true
    make -C "$TOOLS_DIR" CC="$CC_BIN" all
fi

echo "[stage13_soft] build done -> $OUT_DIR/netdev_stage13_soft.ko"