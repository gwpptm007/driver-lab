#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/platform_matrix.md"
mkdir -p "$ROOT_DIR/output"
"$ROOT_DIR/scripts/check_platform_env.sh" >/dev/null
TARGET_PROFILE=host "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null
TARGET_PROFILE=qemu-x86_64 "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null
TARGET_PROFILE=qemu-arm64 "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null

# shellcheck source=/dev/null
source "$ROOT_DIR/output/host_env_stage06.env"
source "$ROOT_DIR/output/resolved_host.env"
HOST_TARGET_ARCH=$TARGET_ARCH
HOST_RUN_MODE=$RUN_MODE
HOST_KDIR_VAL=$KDIR
source "$ROOT_DIR/output/resolved_qemu-x86_64.env"
X86_TARGET_ARCH=$TARGET_ARCH
X86_RUN_MODE=$RUN_MODE
X86_QEMU_BIN=$QEMU_BIN
X86_KDIR_VAL=$KDIR
X86_KERNEL_IMAGE=$KERNEL_IMAGE
X86_ROOTFS_IMAGE=$ROOTFS_IMAGE
source "$ROOT_DIR/output/resolved_qemu-arm64.env"
ARM_TARGET_ARCH=$TARGET_ARCH
ARM_RUN_MODE=$RUN_MODE
ARM_QEMU_BIN=$QEMU_BIN
ARM_CROSS_COMPILE=$CROSS_COMPILE
ARM_KDIR_VAL=$KDIR
ARM_KERNEL_IMAGE=$KERNEL_IMAGE
ARM_ROOTFS_IMAGE=$ROOTFS_IMAGE

{
    echo '# platform_matrix'
    echo
    echo '| profile | arch | run mode | qemu | cross toolchain | kernel build dir | kernel image | rootfs |'
    echo '|---|---|---|---|---|---|---|---|'
    printf '| host | %s | %s | n/a | native gcc | %s | n/a | n/a |\n' \
        "$HOST_TARGET_ARCH" "$HOST_RUN_MODE" "${HOST_KDIR_VAL:-n/a}"
    printf '| qemu-x86_64 | %s | %s | %s | native gcc | %s | %s | %s |\n' \
        "$X86_TARGET_ARCH" "$X86_RUN_MODE" "${X86_QEMU_BIN:-n/a}" \
        "${X86_KDIR_VAL:-n/a}" "${X86_KERNEL_IMAGE:-n/a}" "${X86_ROOTFS_IMAGE:-n/a}"
    printf '| qemu-arm64 | %s | %s | %s | %s | %s | %s | %s |\n' \
        "$ARM_TARGET_ARCH" "$ARM_RUN_MODE" "${ARM_QEMU_BIN:-n/a}" \
        "${ARM_CROSS_COMPILE:-n/a}" "${ARM_KDIR_VAL:-n/a}" \
        "${ARM_KERNEL_IMAGE:-n/a}" "${ARM_ROOTFS_IMAGE:-n/a}"
    echo
    echo '## host 能力摘要'
    echo
    printf -- '- qemu-system-x86_64 available: %s\n' "${HAVE_QEMU_X86}"
    printf -- '- qemu-system-aarch64 available: %s\n' "${HAVE_QEMU_ARM64}"
    printf -- '- aarch64-linux-gnu-gcc available: %s\n' "${HAVE_AARCH64_GCC}"
    echo
    echo '## 说明'
    echo
    echo '- host 行只代表宿主环境原生构建/检查能力'
    echo '- qemu-x86_64 行主要用于 run 方式与参数整理'
    echo '- qemu-arm64 行是 stage06 的最终重点'
} > "$OUT_FILE"

echo "[stage06] platform matrix -> $OUT_FILE"
