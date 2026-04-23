#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:-$(./scripts/create_round_workspace.sh round1-arch)}
./scripts/collect_virtio_net_symbols.sh "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}" "$OUT_DIR" >/dev/null
./scripts/build_function_index.sh "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}" "$OUT_DIR/virtio_net_function_index.md" >/dev/null
./scripts/build_grouped_function_index.sh "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}" "$OUT_DIR/virtio_net_grouped_index.md" >/dev/null
./scripts/extract_probe_path.sh "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}" "$OUT_DIR/probe_path_snippets.txt" >/dev/null
echo "Round1 output: $OUT_DIR"
