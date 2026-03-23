#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day31] 开始构建 day31 独立 initramfs'
bash "${DAY31_ROOT}/scripts/03_build_tools.sh"
if [ ! -x "${GUEST_LSPCI_BIN}" ]; then
  echo '[day31][WARN] 未发现 guest lspci，尝试自动构建。'
  bash "${DAY31_ROOT}/scripts/02_build_guest_lspci.sh"
fi
# Day31 的 run 流程默认总是重编模块，避免源码更新但旧 ko 残留。
bash "${DAY31_ROOT}/scripts/09_build_day31_module.sh"
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day31_edu_bench_tool" day31_edu_bench_tool
require_file "${DAY31_ROOT}/driver/day31_edu_bench.ko" day31_edu_bench.ko

ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"/*
mkdir -p "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/root" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/tmp"

cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
# busybox applet 链接必须提前补齐，否则 /init 里会出现 mount/cat 等命令不存在的问题。
for cmd in sh mount cat sed head sleep insmod mknod dmesg grep poweroff reboot echo true; do
  ln -sf busybox "${ROOTFS_DIR}/bin/$cmd"
done

cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day31_edu_bench_tool" "${ROOTFS_DIR}/bin/day31_edu_bench_tool"
cp -f "${DAY31_ROOT}/driver/day31_edu_bench.ko" "${ROOTFS_DIR}/root/day31_edu_bench.ko"
cp -f "${DAY31_ROOT}/guest/init.day31" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3

(
  cd "${ROOTFS_DIR}"
  find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)
require_file "${ROOTFS_IMG}" ROOTFS_IMG
echo "[day31] day31 initramfs 构建完成：${ROOTFS_IMG}"
