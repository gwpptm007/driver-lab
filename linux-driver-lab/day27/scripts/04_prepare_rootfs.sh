#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 组装最小 initramfs。Day27 的关键是：
# 1. 把 lspci / day27_edu_tool / day27_edu_loop.ko 都打进去；
# 2. guest 启动后无需手工操作，直接跑 200 次循环并产出 marker 日志。

echo '[day27] 开始构建 day27 独立 initramfs'
bash "${DAY27_ROOT}/scripts/03_build_tools.sh"
if [ ! -x "${GUEST_LSPCI_BIN}" ]; then
  echo '[day27][WARN] 未发现 guest lspci，尝试自动构建。'
  bash "${DAY27_ROOT}/scripts/02_build_guest_lspci.sh"
fi
if [ ! -f "${DAY27_ROOT}/driver/day27_edu_loop.ko" ]; then
  echo '[day27][WARN] 未发现 day27_edu_loop.ko，先自动构建模块。'
  bash "${DAY27_ROOT}/scripts/09_build_day27_module.sh"
fi
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day27_edu_tool" day27_edu_tool
require_file "${DAY27_ROOT}/driver/day27_edu_loop.ko" day27_edu_loop.ko
ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"/*
mkdir -p "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/root" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/tmp"
cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
ln -sf busybox "${ROOTFS_DIR}/bin/sh"
cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day27_edu_tool" "${ROOTFS_DIR}/bin/day27_edu_tool"
cp -f "${DAY27_ROOT}/driver/day27_edu_loop.ko" "${ROOTFS_DIR}/root/day27_edu_loop.ko"
cp -f "${DAY27_ROOT}/guest/init.day27" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"
mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3
(
  cd "${ROOTFS_DIR}"
  find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)
require_file "${ROOTFS_IMG}" ROOTFS_IMG
echo "[day27] day27 initramfs 构建完成：${ROOTFS_IMG}"
