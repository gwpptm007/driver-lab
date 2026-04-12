#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENV_FILE="$ROOT_DIR/output/host_env_stage05.env"
REPORT_FILE="$ROOT_DIR/output/stage05_report.md"
[[ -f "$ENV_FILE" ]] || "$ROOT_DIR/scripts/check_host_env.sh"
source "$ENV_FILE"
MAP_READY=no
COMPARE_READY=no
PLATFORM_READY=no
[[ -f "$ROOT_DIR/output/virtio_net_map.md" ]] && MAP_READY=yes
[[ -f "$ROOT_DIR/output/stage04_vs_virtio_report.md" ]] && COMPARE_READY=yes
[[ -f "$ROOT_DIR/output/platform_matrix.md" ]] && PLATFORM_READY=yes
cat > "$REPORT_FILE" <<EOF
# stage05_virtio_param report

- Host kernel: ${HOST_KERNEL}
- gcc available: ${HAVE_GCC}
- qemu-system-x86_64 available: ${HAVE_QEMU_X86}
- qemu-system-aarch64 available: ${HAVE_QEMU_ARM64}
- aarch64-linux-gnu-gcc available: ${HAVE_AARCH64_GCC}
- virtio_net.c found: ${HAVE_VIRTIO_SOURCE}
- virtio_net.c path: ${VIRTIO_NET_SOURCE}

## 输出状态

- MAP_READY=${MAP_READY}
- COMPARE_READY=${COMPARE_READY}
- PLATFORM_READY=${PLATFORM_READY}

## readiness

- STAGE05_DOC_READY=yes
- STAGE05_SOURCE_ANCHORED=${HAVE_VIRTIO_SOURCE}
- STAGE05_MIGRATION_PREP=$( [[ "$PLATFORM_READY" == yes ]] && echo yes || echo no )
EOF
echo "[stage05] generated report -> $REPORT_FILE"
