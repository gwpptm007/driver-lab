#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:-$(./scripts/create_round_workspace.sh round2-txrx)}
./scripts/extract_tx_path.sh "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}" "$OUT_DIR/tx_path_snippets.txt" >/dev/null
./scripts/extract_rx_path.sh "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}" "$OUT_DIR/rx_path_snippets.txt" >/dev/null
./scripts/trace_virtio_net_basic.sh > "$OUT_DIR/trace_sample.txt" || true
echo "Round2 output: $OUT_DIR"
