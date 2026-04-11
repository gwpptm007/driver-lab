#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day29] 开始构建 day29 独立 initramfs'
# Day29 采用“最小独立 rootfs”思路：
# 只放 BusyBox、guest 工具、驱动模块和 init 脚本。
# 好处是问题面很小，串口日志更干净，records 也更容易解释。
bash "${DAY29_ROOT}/scripts/03_build_tools.sh"
if [ ! -x "${GUEST_LSPCI_BIN}" ]; then
  echo '[day29][WARN] 未发现 guest lspci，尝试自动构建。'
  bash "${DAY29_ROOT}/scripts/02_build_guest_lspci.sh"
fi
if [ ! -f "${DAY29_ROOT}/driver/day29_edu_dma.ko" ]; then
  echo '[day29][WARN] 未发现 day29_edu_dma.ko，先自动构建模块。'
  bash "${DAY29_ROOT}/scripts/09_build_day29_module.sh"
fi
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day29_edu_dma_tool" day29_edu_dma_tool
require_file "${DAY29_ROOT}/driver/day29_edu_dma.ko" day29_edu_dma.ko
ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"/*
mkdir -p "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/root" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/tmp"
cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
# 注意：BUSYBOX_BIN 是 arm64 guest 二进制，宿主机上不能执行 busybox --install。
# 因此这里手工为 guest /init 会用到的 applet 建链接，否则 /init 中 mount/cat/sed 等命令会找不到。
for cmd in sh mount cat sed head sleep insmod mknod dmesg grep poweroff reboot echo true; do
  ln -sf busybox "${ROOTFS_DIR}/bin/$cmd"
done
cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day29_edu_dma_tool" "${ROOTFS_DIR}/bin/day29_edu_dma_tool"
cp -f "${DAY29_ROOT}/driver/day29_edu_dma.ko" "${ROOTFS_DIR}/root/day29_edu_dma.ko"
cp -f "${DAY29_ROOT}/guest/init.day29" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"
mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3
(
  cd "${ROOTFS_DIR}"
  find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)
require_file "${ROOTFS_IMG}" ROOTFS_IMG
echo "[day29] day29 initramfs 构建完成：${ROOTFS_IMG}"
