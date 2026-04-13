content = """#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/env/stage06_arm64_migration.env"
ENV_FILE="$ROOT_DIR/output/host_env_stage06.env"
[[ -f "$ENV_FILE" ]] || "$ROOT_DIR/scripts/check_platform_env.sh"
source "$ENV_FILE"

PROFILE=${TARGET_PROFILE:-host}
OUT_FILE="$ROOT_DIR/output/resolved_${PROFILE}.env"
STAGE04_DIR_DEFAULT=$(cd "$ROOT_DIR/../stage04_ring_dma" && pwd)

quote() {
    printf '%q' "$1"
}

case "$PROFILE" in
    host)
        TARGET_ARCH=host
        RUN_MODE=host
        QEMU_BIN=
        CROSS_COMPILE=
        KDIR=${HOST_KDIR}
        KERNEL_BUILD_DIR=${HOST_KDIR}
        KERNEL_IMAGE=
        ROOTFS_IMAGE_RESOLVED=
        QEMU_MACHINE=
        QEMU_CPU=
        ;;
    qemu-x86_64)
        TARGET_ARCH=x86_64
        RUN_MODE=qemu-x86_64
        QEMU_BIN=${QEMU_X86_BIN:-}
        CROSS_COMPILE=
        KDIR=
        KERNEL_BUILD_DIR=
        KERNEL_IMAGE=
        ROOTFS_IMAGE_RESOLVED=${ROOTFS_IMAGE:-}
        QEMU_MACHINE=pc
        QEMU_CPU=host
        ;;
    qemu-arm64)
        TARGET_ARCH=arm64
        RUN_MODE=qemu-arm64
        QEMU_BIN=${QEMU_ARM64_BIN:-}
        CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
        KDIR=${ARM64_BUILD_DIR:-}
        KERNEL_BUILD_DIR=${ARM64_BUILD_DIR:-}
        KERNEL_IMAGE=${ARM64_IMAGE:-}
        ROOTFS_IMAGE_RESOLVED=${ROOTFS_IMAGE:-}
        QEMU_MACHINE=virt
        QEMU_CPU=cortex-a57
        ;;
    *)
        echo "[stage06] unknown TARGET_PROFILE=$PROFILE" >&2
        exit 2
        ;;
esac

{
    printf 'TARGET_PROFILE=%q\n' "$PROFILE"
    printf 'TARGET_ARCH=%q\n' "$TARGET_ARCH"
    printf 'RUN_MODE=%q\n' "$RUN_MODE"
    printf 'QEMU_BIN=%q\n' "$QEMU_BIN"
    printf 'CROSS_COMPILE=%q\n' "$CROSS_COMPILE"
    printf 'KDIR=%q\n' "$KDIR"
    printf 'KERNEL_BUILD_DIR=%q\n' "$KERNEL_BUILD_DIR"
    printf 'KERNEL_IMAGE=%q\n' "$KERNEL_IMAGE"
    printf 'ROOTFS_IMAGE=%q\n' "$ROOTFS_IMAGE_RESOLVED"
    printf 'QEMU_MACHINE=%q\n' "$QEMU_MACHINE"
    printf 'QEMU_CPU=%q\n' "$QEMU_CPU"
    printf 'QEMU_MEMORY=%q\n' "${QEMU_MEMORY:-512}"
    printf 'STAGE04_DIR=%q\n' "${STAGE04_DIR:-$STAGE04_DIR_DEFAULT}"
    printf 'IFNAME=%q\n' "${IFNAME:-nds4}"
    printf 'RING_SIZE=%q\n' "${RING_SIZE:-64}"
    printf 'NAPI_WEIGHT=%q\n' "${NAPI_WEIGHT:-16}"
    printf 'RX_BUF_SIZE=%q\n' "${RX_BUF_SIZE:-2048}"
} > "$OUT_FILE"

echo "[stage06] resolved $PROFILE -> $OUT_FILE"
"""
with open('e:/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage06_arm64_migration/scripts/resolve_platform_env.sh', 'w') as f:
    f.write(content)
print('written')