#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN='/usr/bin/qemu-system-aarch64'
KERNEL_IMAGE=''
ROOTFS_IMAGE=''

if [[ -z "$QEMU_BIN" ]]; then
    echo "missing QEMU_BIN"
    exit 2
fi
if [[ -z "$KERNEL_IMAGE" ]]; then
    echo "missing KERNEL_IMAGE"
    exit 2
fi
if [[ -z "$ROOTFS_IMAGE" ]]; then
    echo "missing ROOTFS_IMAGE"
    exit 2
fi

exec "$QEMU_BIN"     -machine virt     -cpu cortex-a57     -m 512M     -nographic     -kernel "$KERNEL_IMAGE"     -initrd "$ROOTFS_IMAGE"     -append "console=ttyAMA0 rdinit=/init"
