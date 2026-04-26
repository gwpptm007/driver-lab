#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:?usage: $0 <kernel-src> <out-patch>}
OUT_FILE=${2:?usage: $0 <kernel-src> <out-patch>}
git -C "$KERNEL_SRC" diff > "$OUT_FILE"
echo "$OUT_FILE"
