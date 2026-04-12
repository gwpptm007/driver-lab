#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/host_env_stage05.env"
mkdir -p "$ROOT_DIR/output"
source "$ROOT_DIR/scripts/lib_stage05.sh"

VIRTIO_PATH=""
if VIRTIO_PATH=$(stage05_find_virtio_net_source 2>/dev/null); then
    HAVE_VIRTIO_SOURCE=yes
else
    HAVE_VIRTIO_SOURCE=no
fi
QEMU_X86_BIN=${QEMU_SYSTEM_X86_64:-qemu-system-x86_64}
QEMU_ARM_BIN=${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}
{
    printf "HOST_UNAME=%q
" "$(uname -a | sed 's/[[:space:]]\+/ /g')"
    echo "HOST_KERNEL=$(uname -r)"
    echo "HOST_CC=${HOST_CC:-gcc}"
    printf "KERNEL_SOURCE_ROOT=%q
" "${KERNEL_SOURCE_ROOT:-}"
    printf "VIRTIO_NET_SOURCE=%q
" "$VIRTIO_PATH"
    echo "HAVE_VIRTIO_SOURCE=$HAVE_VIRTIO_SOURCE"
    echo "HAVE_GCC=$(stage05_has_cmd gcc)"
    echo "HAVE_MAKE=$(stage05_has_cmd make)"
    echo "HAVE_QEMU_X86=$(stage05_has_cmd "$QEMU_X86_BIN")"
    echo "HAVE_QEMU_ARM64=$(stage05_has_cmd "$QEMU_ARM_BIN")"
    echo "HAVE_AARCH64_GCC=$(stage05_has_cmd aarch64-linux-gnu-gcc)"
} > "$OUT_FILE"
echo "[stage05] host env -> $OUT_FILE"
