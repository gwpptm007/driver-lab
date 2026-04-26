#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_FILE=${2:-patches/0001-local-working-tree.patch}
git -C "$KERNEL_SRC" diff > "$OUT_FILE"
echo "$OUT_FILE"
