#!/usr/bin/env bash
set -euo pipefail

KERNEL_SRC=${1:-${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}}
OUT_FILE=${2:-reports/virtio_net_grouped_function_index.md}
SRC_FILE="$KERNEL_SRC/drivers/net/virtio_net.c"

[[ -f "$SRC_FILE" ]] || { echo "not found: $SRC_FILE" >&2; exit 1; }
mkdir -p "$(dirname "$OUT_FILE")"

{
  echo "# virtio_net grouped function index"
  echo
  echo "## 初始化 / 设备模型"
  grep -nE 'probe|remove|virtio_driver|open|close|init|free' "$SRC_FILE" || true
  echo
  echo "## TX"
  grep -nE 'start_xmit|xmit|send|sq->|kick' "$SRC_FILE" || true
  echo
  echo "## RX / NAPI"
  grep -nE 'poll|receive|recv|napi|fill|refill|gro|xdp' "$SRC_FILE" || true
  echo
  echo "## 控制面 / feature / ethtool"
  grep -nE 'feature|ethtool|stats|channels|set_.*features|get_.*stats' "$SRC_FILE" || true
} > "$OUT_FILE"

echo "$OUT_FILE"
