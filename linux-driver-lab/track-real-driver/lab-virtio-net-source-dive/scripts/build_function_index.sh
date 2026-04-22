#!/usr/bin/env bash
set -euo pipefail
KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_FILE=${2:-reports/virtio_net_function_index.md}
SRC_FILE="$KERNEL_SRC/drivers/net/virtio_net.c"
[[ -f "$SRC_FILE" ]] || { echo "not found: $SRC_FILE" >&2; exit 1; }
mkdir -p "$(dirname "$OUT_FILE")"
{
  echo "# virtio_net function index"
  echo
  echo '```text'
  grep -nE '^(static )?(int|void|bool|u16|u32|u64|netdev_tx_t) [a-zA-Z0-9_]+\(' "$SRC_FILE"
  echo '```'
} > "$OUT_FILE"
echo "$OUT_FILE"
