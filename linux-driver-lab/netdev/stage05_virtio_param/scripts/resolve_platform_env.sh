#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT_DIR/output"
source "$ROOT_DIR/scripts/lib_stage05.sh"
ARCH=${TARGET_ARCH:-host}
MODE=${RUN_MODE:-host}
RESOLVED_CROSS=$(stage05_resolve_cross_compile)
RESOLVED_QEMU=$(stage05_resolve_qemu_bin)
out_name="resolved_${ARCH}"
[[ "$MODE" != host ]] && out_name+="_${MODE}"
OUT_FILE="$ROOT_DIR/output/${out_name}.env"
{
    echo "TARGET_ARCH=$ARCH"
    echo "RUN_MODE=$MODE"
    printf "HOST_CC=%q
" "${HOST_CC:-gcc}"
    printf "CROSS_COMPILE=%q
" "$RESOLVED_CROSS"
    printf "QEMU_BIN=%q
" "$RESOLVED_QEMU"
    printf "KERNEL_SOURCE_ROOT=%q
" "${KERNEL_SOURCE_ROOT:-}"
    printf "KERNEL_BUILD_DIR=%q
" "${KERNEL_BUILD_DIR:-}"
    printf "KERNEL_IMAGE=%q
" "${KERNEL_IMAGE:-}"
    printf "ROOTFS_IMAGE=%q
" "${ROOTFS_IMAGE:-}"
    printf "VIRTIO_NET_SOURCE=%q
" "${VIRTIO_NET_SOURCE:-}"
} > "$OUT_FILE"
echo "[stage05] resolved env -> $OUT_FILE"
