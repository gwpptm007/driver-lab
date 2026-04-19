#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# run.sh — 启动 QEMU 并加载 stage10 MSI-X 驱动
#
# QEMU 配置说明:
#   - ivshmem: 共享内存 PCI 设备，支持 MSI 中断
#     vectors=N 设置 MSI 向量数（对应 queue 数量）
#     memdev= 指定共享内存后端
#   - stage10 driver 通过 pci_alloc_irq_vectors 分配 per-queue MSI-X vectors
#   - /proc/interrupts 中可看到 stage10-q0, stage10-q1 等 IRQ 条目
#
# 如需修改 ivshmem 参数：
#   IVSHMEM_VECTORS=4  ./scripts/run.sh reload

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
KO="$ROOT_DIR/output/netdev_stage10.ko"
ACTION=${1:-reload}

IFNAME=${IFNAME:-nds10}
NUM_QUEUES=${NUM_QUEUES:-2}
RING_SIZE=${RING_SIZE:-128}
NAPI_WEIGHT=${NAPI_WEIGHT:-64}
RX_BUF_SIZE=${RX_BUF_SIZE:-2048}
BACKEND_BATCH=${BACKEND_BATCH:-64}
IVSHMEM_VECTORS=${IVSHMEM_VECTORS:-4}

# QEMU 路径（可在环境变量覆盖）
QEMU_BIN=${QEMU_BIN:-qemu-system-x86_64}
KERNEL_IMG=${KERNEL_IMG:-/boot/vmlinuz}
ROOTFS_IMG=${ROOTFS_IMG:-"$ROOT_DIR/../../../kernel-src/busybox-1.36.1/output/x86/_install"}
ACCEL=${ACCEL:-kvm}

# ivshmem shared memory file
IVSHMEM_SIZE=${IVSHMEM_SIZE:-8388608}  # 8MB
IVSHMEM_PATH="/dev/shm/stage10-shm"

is_loaded() {
    lsmod | awk '{print $1}' | grep -qx netdev_stage10
}

case "$ACTION" in
    load)
        test -f "$KO" || { echo "[stage10] missing $KO, run scripts/build.sh first" >&2; exit 1; }

        # 创建共享内存文件（ivshmem 需要）
        touch "$IVSHMEM_PATH" && truncate -s "$IVSHMEM_SIZE" "$IVSHMEM_PATH"

        # 启动 QEMU（后台），加载模块
        # ivshmem 配置：
        #   -device ivshmem,memdev=stage10_shmem,vectors=4
        #     分配 4 个 MSI 向量（对应 stage10 的 num_queues）
        #   -object memory-backend-file,id=stage10_shmem,mem-path=$IVSHMEM_PATH,...
        #     提供共享内存后端
        $QEMU_BIN \
            -enable-kvm \
            -m 2G \
            -kernel "$KERNEL_IMG" \
            -append "console=ttyS0 root=/dev/sda ro" \
            -hda "$ROOTFS_IMG" \
            -fsdev local,id=fs0,path="$ROOT_DIR/../../../kernel-src/busybox-1.36.1/output/x86/_install,security_model=mapped-file \
            -device virtio-serial-pci \
            -device virtconsole,chardev=con \
            -object memory-backend-file,id=stage10_shmem,mem-path="$IVSHMEM_PATH",size="${IVSHMEM_SIZE}",share=on \
            -device ivshmem,memdev=stage10_shmem,vectors="$IVSHMEM_VECTORS" \
            -nographic \
            -device e1000,netdev=net0 \
            -netdev user,id=net0 \
            -machine "$ACCEL" \
            &
        QEMU_PID=$!
        echo "[stage10] QEMU started (PID=$QEMU_PID)"
        sleep 3

        # 在 QEMU 内加载模块
        # 注意：实际通过 serial console 或 ssh 操作
        echo "[stage10] QEMU running. Connect to operate."
        echo "[stage10] To load module inside QEMU:"
        echo "  insmod /root/netdev_stage10.ko ifname=nds10 num_queues=$NUM_QUEUES"
        ;;
    unload)
        if is_loaded; then
            sudo rmmod netdev_stage10
            echo "[stage10] unloaded"
        else
            echo "[stage10] module not loaded"
        fi
        ;;
    reload)
        "$0" unload 2>/dev/null || true
        "$0" load
        ;;
    status)
        is_loaded && echo "[stage10] loaded" || echo "[stage10] not loaded"
        ls /sys/kernel/debug/netdev_stage10/ 2>/dev/null || echo "[stage10] debugfs not mounted"
        ;;
    *)
        echo "Usage: $0 [load|unload|reload|status]" >&2
        echo ""
        echo "Environment variables:"
        echo "  QEMU_BIN=$QEMU_BIN"
        echo "  IVSHMEM_VECTORS=$IVSHMEM_VECTORS (MSI-X vectors, should >= num_queues)"
        echo "  NUM_QUEUES=$NUM_QUEUES"
        exit 1
        ;;
esac
