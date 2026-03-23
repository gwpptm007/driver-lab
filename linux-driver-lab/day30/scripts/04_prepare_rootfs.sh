#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day30] 开始构建 day30 独立 initramfs'
# rootfs 里不只是拷一个 busybox 二进制，还要把 guest /init 会直接调用到的
# applet 链接补齐，否则会出现 mount/insmod/cat 等命令不存在，导致 init 提前退出。
bash "${DAY30_ROOT}/scripts/03_build_tools.sh"
if [ ! -x "${GUEST_LSPCI_BIN}" ]; then
  echo '[day30][WARN] 未发现 guest lspci，尝试自动构建。'
  bash "${DAY30_ROOT}/scripts/02_build_guest_lspci.sh"
fi
if [ ! -f "${DAY30_ROOT}/driver/day30_edu_mmap.ko" ]; then
  echo '[day30][WARN] 未发现 day30_edu_mmap.ko，先自动构建模块。'
  bash "${DAY30_ROOT}/scripts/09_build_day30_module.sh"
fi
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day30_edu_mmap_tool" day30_edu_mmap_tool
require_file "${DAY30_ROOT}/driver/day30_edu_mmap.ko" day30_edu_mmap.ko

ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"/*
mkdir -p "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/root" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/tmp"

cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
for cmd in sh mount cat sed head sleep insmod mknod dmesg grep poweroff reboot echo true; do
  ln -sf busybox "${ROOTFS_DIR}/bin/$cmd"
done

cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day30_edu_mmap_tool" "${ROOTFS_DIR}/bin/day30_edu_mmap_tool"
cp -f "${DAY30_ROOT}/driver/day30_edu_mmap.ko" "${ROOTFS_DIR}/root/day30_edu_mmap.ko"
cp -f "${DAY30_ROOT}/guest/init.day30" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3

(
  cd "${ROOTFS_DIR}"
  find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)
require_file "${ROOTFS_IMG}" ROOTFS_IMG
echo "[day30] day30 initramfs 构建完成：${ROOTFS_IMG}"
