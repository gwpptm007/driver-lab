#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day34] 开始构建 day34 独立 initramfs'
bash "${DAY34_ROOT}/scripts/03_build_tools.sh"
if [ ! -x "${GUEST_LSPCI_BIN}" ]; then
  echo '[day34][WARN] 未发现 guest lspci，尝试自动构建。'
  bash "${DAY34_ROOT}/scripts/02_build_guest_lspci.sh"
fi
bash "${DAY34_ROOT}/scripts/09_build_day34_module.sh"
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day34_edu_stability_tool" day34_edu_stability_tool
require_file "${DAY34_ROOT}/driver/day34_edu_stability.ko" day34_edu_stability.ko

ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"/*
mkdir -p "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/root" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/tmp"

cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
for cmd in sh mount cat sed head sleep insmod rmmod mknod dmesg grep poweroff reboot echo true mkdir; do
  ln -sf busybox "${ROOTFS_DIR}/bin/$cmd"
done
cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day34_edu_stability_tool" "${ROOTFS_DIR}/bin/day34_edu_stability_tool"
cp -f "${DAY34_ROOT}/driver/day34_edu_stability.ko" "${ROOTFS_DIR}/root/day34_edu_stability.ko"
cp -f "${DAY34_ROOT}/guest/init.day34" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3
(
  cd "${ROOTFS_DIR}"
  find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)
require_file "${ROOTFS_IMG}" ROOTFS_IMG
echo "[day34] day34 initramfs 构建完成：${ROOTFS_IMG}"
