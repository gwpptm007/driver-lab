#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_DIR=${2:-records/manual-source-dive}
mkdir -p "$OUT_DIR"
SRC_FILE="$KERNEL_SRC/drivers/net/virtio_net.c"
[[ -f "$SRC_FILE" ]] || { echo "not found: $SRC_FILE" >&2; exit 1; }
grep -nE '^(static )?(int|void|bool|u16|u32|u64|netdev_tx_t) [a-zA-Z0-9_]+\(' "$SRC_FILE" > "$OUT_DIR/virtio_net_symbols.txt"
echo "$OUT_DIR/virtio_net_symbols.txt"
