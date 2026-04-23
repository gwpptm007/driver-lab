#!/usr/bin/env bash
set -euo pipefail

KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_DIR=${2:-records/manual-source-dive}
SRC_FILE="$KERNEL_SRC/drivers/net/virtio_net.c"

[[ -f "$SRC_FILE" ]] || { echo "not found: $SRC_FILE" >&2; exit 1; }
mkdir -p "$OUT_DIR"

{
  echo "# rx related symbols"
  echo
  grep -nE 'poll|receive|recv|rq->|napi|refill|fill|xdp|gro' "$SRC_FILE" || true
  echo
  for pat in 'virtnet_poll' 'receive' 'try_fill' 'xdp'; do
    ln=$(grep -n "$pat" "$SRC_FILE" | head -n1 | cut -d: -f1 || true)
    if [[ -n "${ln:-}" ]]; then
      start=$(( ln > 20 ? ln - 20 : 1 ))
      end=$(( ln + 100 ))
      echo "## $pat @ line $ln"
      sed -n "${start},${end}p" "$SRC_FILE"
      echo
    fi
  done
} > "$OUT_DIR/rx_path_snippets.txt"

echo "$OUT_DIR/rx_path_snippets.txt"
