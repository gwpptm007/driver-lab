#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_DIR=${2:-records/manual-e1000e-source-dive}
mkdir -p "$OUT_DIR"

for f in "$KERNEL_SRC/drivers/net/ethernet/intel/e1000e/netdev.c"          "$KERNEL_SRC/drivers/net/ethernet/intel/e1000/e1000_main.c"; do
    if [[ -f "$f" ]]; then
        grep -nE '^(static )?(int|void|bool|u16|u32|u64|netdev_tx_t) [a-zA-Z0-9_]+\(' "$f" >> "$OUT_DIR/e1000e_symbols.txt" || true
    fi
done

echo "$OUT_DIR/e1000e_symbols.txt"
