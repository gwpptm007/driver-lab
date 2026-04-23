#!/usr/bin/env bash
set -euo pipefail

KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_DIR=${2:-records/manual-source-dive}
SRC_FILE="$KERNEL_SRC/drivers/net/virtio_net.c"

[[ -f "$SRC_FILE" ]] || { echo "not found: $SRC_FILE" >&2; exit 1; }
mkdir -p "$OUT_DIR"

{
  echo "# probe/remove candidates"
  echo
  grep -nE 'virtio_driver|probe|remove|virtnet_.*probe|virtnet_.*remove|register_netdev|unregister_netdev|alloc_etherdev' "$SRC_FILE" || true
  echo
  echo "# nearby blocks"
  for pat in 'virtnet_probe' 'virtnet_remove' 'register_netdev' 'unregister_netdev'; do
    ln=$(grep -n "$pat" "$SRC_FILE" | head -n1 | cut -d: -f1 || true)
    if [[ -n "${ln:-}" ]]; then
      start=$(( ln > 20 ? ln - 20 : 1 ))
      end=$(( ln + 60 ))
      echo "## $pat @ line $ln"
      sed -n "${start},${end}p" "$SRC_FILE"
      echo
    fi
  done
} > "$OUT_DIR/probe_path_snippets.txt"

echo "$OUT_DIR/probe_path_snippets.txt"
