#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_FILE=${2:-records/manual-e1000e-source-dive/txrx_seed.txt}
mkdir -p "$(dirname "$OUT_FILE")"

{
  echo "# tx/rx seed"
  echo
  for f in "$KERNEL_SRC/drivers/net/ethernet/intel/e1000e/netdev.c"            "$KERNEL_SRC/drivers/net/ethernet/intel/e1000/e1000_main.c"; do
      if [[ -f "$f" ]]; then
          echo "## $f"
          grep -nE 'xmit|napi|poll|rx|tx|clean|irq|interrupt' "$f" | head -n 160 || true
          echo
      fi
  done
} > "$OUT_FILE"

echo "$OUT_FILE"
