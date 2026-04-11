#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 组装一个最小 initramfs：
# - BusyBox 提供最基础的 shell / mount / grep / insmod / rmmod 等 applet
# - lspci 用于枚举 EDU 设备
# - day25_irq_tool 用于用户态触发和读取中断状态
# - day25_edu_irq.ko 是本轮实验的核心驱动模块
# - /init 即 guest 自动执行流程

echo "[day25] 开始构建 day25 独立 initramfs"
"${DAY25_ROOT}/scripts/03_build_tools.sh"

if [ ! -x "${GUEST_LSPCI_BIN:-}" ]; then
    echo "[day25][WARN] 未发现 guest lspci，尝试自动构建。"
    "${DAY25_ROOT}/scripts/02_build_guest_lspci.sh"
fi

require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day25_irq_tool" GUEST_TOOL_BIN
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_file "${DAY25_ROOT}/driver/day25_edu_irq.ko" "day25 module"

rm -rf "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}"/{bin,dev,etc,proc,sys,tmp,root,lib,usr/bin}

cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
chmod +x "${ROOTFS_DIR}/bin/busybox"
for app in sh mount umount mkdir ls cat echo dmesg grep cut sleep poweroff insmod rmmod mknod chmod chown head tr; do
    ln -sf /bin/busybox "${ROOTFS_DIR}/bin/${app}"
done

cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
chmod +x "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day25_irq_tool" "${ROOTFS_DIR}/bin/day25_irq_tool"
chmod +x "${ROOTFS_DIR}/bin/day25_irq_tool"
cp -f "${DAY25_ROOT}/driver/day25_edu_irq.ko" "${ROOTFS_DIR}/root/day25_edu_irq.ko"
chmod 0644 "${ROOTFS_DIR}/root/day25_edu_irq.ko"
cp -f "${DAY25_ROOT}/guest/init.day25" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3

(
    cd "${ROOTFS_DIR}"
    find . -print0 | cpio --null -ov --format=newc 2>/dev/null | gzip -9 > "${ROOTFS_IMG}"
)

echo "[day25] day25 initramfs 构建完成：${ROOTFS_IMG}"
