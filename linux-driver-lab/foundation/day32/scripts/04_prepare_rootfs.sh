#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day32] 开始构建 day32 独立 initramfs'
bash "${DAY32_ROOT}/scripts/03_build_tools.sh"
if [ ! -x "${GUEST_LSPCI_BIN}" ]; then
  echo '[day32][WARN] 未发现 guest lspci，尝试自动构建。'
  bash "${DAY32_ROOT}/scripts/02_build_guest_lspci.sh"
fi
bash "${DAY32_ROOT}/scripts/09_build_day32_module.sh"
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day32_edu_perf_tool" day32_edu_perf_tool
require_file "${DAY32_ROOT}/driver/day32_edu_perf.ko" day32_edu_perf.ko

ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"/*
mkdir -p "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/root" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/tmp"

cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
for cmd in sh mount cat sed head sleep insmod mknod dmesg grep poweroff reboot echo true; do
  ln -sf busybox "${ROOTFS_DIR}/bin/$cmd"
done
cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day32_edu_perf_tool" "${ROOTFS_DIR}/bin/day32_edu_perf_tool"
cp -f "${DAY32_ROOT}/driver/day32_edu_perf.ko" "${ROOTFS_DIR}/root/day32_edu_perf.ko"
cp -f "${DAY32_ROOT}/guest/init.day32" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3
(
  cd "${ROOTFS_DIR}"
  find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)
require_file "${ROOTFS_IMG}" ROOTFS_IMG
echo "[day32] day32 initramfs 构建完成：${ROOTFS_IMG}"
