#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
DRIVER_DIR="$ROOT_DIR/driver"
TOOLS_DIR="$ROOT_DIR/tools"
OUT_DIR="$ROOT_DIR/output"

KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
CC_BIN=${CC:-gcc}

mkdir -p "$OUT_DIR"

echo "[stage08] build root: $ROOT_DIR"
echo "[stage08] KDIR: $KDIR"

test -d "$KDIR" || { echo "[stage08] missing kernel build dir: $KDIR" >&2; exit 1; }

make -C "$DRIVER_DIR" KDIR="$KDIR" clean >/dev/null || true
make -C "$DRIVER_DIR" KDIR="$KDIR" all
cp -f "$DRIVER_DIR/netdev_stage08.ko" "$OUT_DIR/"

if command -v "$CC_BIN" >/dev/null 2>&1; then
    make -C "$TOOLS_DIR" CC="$CC_BIN" clean >/dev/null || true
    make -C "$TOOLS_DIR" CC="$CC_BIN" all
else
    echo "[stage08] warning: compiler '$CC_BIN' not found, skip userspace tools" >&2
fi

echo "[stage08] build done -> $OUT_DIR/netdev_stage08.ko"
