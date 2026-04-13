#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/host_env_stage06.env"
mkdir -p "$ROOT_DIR/output"

has_cmd() {
    command -v "$1" >/dev/null 2>&1 && echo yes || echo no
}

find_first() {
    for p in "$@"; do
        if [[ -n "$p" && -e "$p" ]]; then
            printf '%s' "$p"
            return 0
        fi
    done
    return 1
}

HOST_KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
X86_QEMU=$(command -v qemu-system-x86_64 || true)
ARM64_QEMU=$(command -v qemu-system-aarch64 || true)
AARCH64_GCC=$(command -v aarch64-linux-gnu-gcc || true)

DEFAULT_KERNEL_SRC_ROOT=${KERNEL_SRC_ROOT:-}
ARM64_IMAGE=$(find_first \
    "${KERNEL_IMAGE:-}" \
    "${DEFAULT_KERNEL_SRC_ROOT}/output/arm64/Image" \
    "/home/wq7/workspace/kernel-src/linux-5.15.10/output/arm64/Image" \
    "") || true
ARM64_BUILD_DIR=$(find_first \
    "${KERNEL_BUILD_DIR:-}" \
    "${DEFAULT_KERNEL_SRC_ROOT}/build/arm64" \
    "/home/wq7/workspace/kernel-src/linux-5.15.10/build/arm64" \
    "") || true
ROOTFS_IMG=$(find_first \
    "${ROOTFS_IMAGE:-}" \
    "/home/wq7/workspace/driver-lab/linux-driver-lab/day35/workdir/rootfs.img" \
    "") || true

{
    printf 'HOST_UNAME=%q\n' "$(uname -a | sed 's/[[:space:]]\+/ /g')"
    printf 'HOST_KERNEL=%q\n' "$(uname -r)"
    printf 'HOST_KDIR=%q\n' "$HOST_KDIR"
    if [[ -d "$HOST_KDIR" ]]; then echo 'HOST_KDIR_OK=yes'; else echo 'HOST_KDIR_OK=no'; fi
    printf 'HAVE_GCC=%q\n' "$(has_cmd gcc)"
    printf 'HAVE_GXX=%q\n' "$(has_cmd g++)"
    printf 'HAVE_MAKE=%q\n' "$(has_cmd make)"
    printf 'HAVE_QEMU_X86=%q\n' "$(has_cmd qemu-system-x86_64)"
    printf 'HAVE_QEMU_ARM64=%q\n' "$(has_cmd qemu-system-aarch64)"
    printf 'HAVE_AARCH64_GCC=%q\n' "$(has_cmd aarch64-linux-gnu-gcc)"
    printf 'QEMU_X86_BIN=%q\n' "$X86_QEMU"
    printf 'QEMU_ARM64_BIN=%q\n' "$ARM64_QEMU"
    printf 'AARCH64_GCC_BIN=%q\n' "$AARCH64_GCC"
    printf 'KERNEL_SRC_ROOT=%q\n' "${DEFAULT_KERNEL_SRC_ROOT}"
    printf 'ARM64_IMAGE=%q\n' "${ARM64_IMAGE:-}"
    printf 'ARM64_BUILD_DIR=%q\n' "${ARM64_BUILD_DIR:-}"
    printf 'ROOTFS_IMAGE=%q\n' "${ROOTFS_IMG:-}"
} > "$OUT_FILE"

echo "[stage06] host env -> $OUT_FILE"
