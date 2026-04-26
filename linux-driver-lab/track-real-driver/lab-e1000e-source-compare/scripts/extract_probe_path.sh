#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_FILE=${2:-records/manual-e1000e-source-dive/probe_path_snippets.txt}
mkdir -p "$(dirname "$OUT_FILE")"

{
  echo "# probe path snippets"
  echo
  for f in "$KERNEL_SRC/drivers/net/ethernet/intel/e1000e/netdev.c"            "$KERNEL_SRC/drivers/net/ethernet/intel/e1000/e1000_main.c"; do
      if [[ -f "$f" ]]; then
          echo "## $f"
          grep -nE 'probe|remove|register_netdev|alloc_etherdev' "$f" | head -n 80 || true
          echo
      fi
  done
} > "$OUT_FILE"

echo "$OUT_FILE"
