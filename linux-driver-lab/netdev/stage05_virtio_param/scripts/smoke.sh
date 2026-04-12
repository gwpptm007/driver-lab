#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"
./scripts/check_host_env.sh
./scripts/collect_virtio_net_map.sh
./scripts/generate_comparison_report.sh
./scripts/generate_platform_matrix.sh
TARGET_ARCH=host RUN_MODE=host ./scripts/resolve_platform_env.sh
TARGET_ARCH=arm64 RUN_MODE=qemu-arm64 ./scripts/resolve_platform_env.sh
./scripts/generate_stage05_report.sh
echo "[stage05] smoke done"
