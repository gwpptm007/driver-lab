#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

ROOTFS="${WORKDIR}/rootfs"
ROOTFS_IMG="${WORKDIR}/rootfs.img"
MODULE_DST="${ROOTFS}/opt/day24"

log "开始构建 day24 独立 initramfs"
require_file BUSYBOX_BIN "${BUSYBOX_BIN}"
require_file MODULE_FILE "${MODULE_FILE}"
if [[ ! -x "${GUEST_LSPCI_BIN}" ]]; then
    warn "未发现 guest lspci，尝试自动构建。"
    "${SCRIPT_DIR}/02_build_guest_lspci.sh"
fi
require_file GUEST_LSPCI_BIN "${GUEST_LSPCI_BIN}"
if [[ ! -x "${TOOL_FILE}" ]]; then
    warn "未发现 day24_mmio_tool，尝试自动构建。"
    "${SCRIPT_DIR}/03_build_day24_tools.sh"
fi
require_file TOOL_FILE "${TOOL_FILE}"

rm -rf "${ROOTFS}"
mkdir -p "${ROOTFS}"/{bin,sbin,proc,sys,dev,tmp,opt/day24}

cp "${BUSYBOX_BIN}" "${ROOTFS}/bin/busybox"
chmod +x "${ROOTFS}/bin/busybox"
for app in sh mount mkdir echo cat dmesg grep sleep poweroff insmod rmmod ls uname chmod; do
    ln -sf /bin/busybox "${ROOTFS}/bin/${app}"
done

cp "${GUEST_LSPCI_BIN}" "${ROOTFS}/bin/lspci"
chmod +x "${ROOTFS}/bin/lspci"
cp "${TOOL_FILE}" "${ROOTFS}/bin/day24_mmio_tool"
chmod +x "${ROOTFS}/bin/day24_mmio_tool"
cp "${MODULE_FILE}" "${MODULE_DST}/day24_ivshmem_mmio.ko"
chmod 0644 "${MODULE_DST}/day24_ivshmem_mmio.ko"
cp "${DAY24_ROOT}/guest/init.day24" "${ROOTFS}/init"
chmod +x "${ROOTFS}/init"

rm -f "${ROOTFS}/dev/console" "${ROOTFS}/dev/null"
mknod -m 600 "${ROOTFS}/dev/console" c 5 1
mknod -m 666 "${ROOTFS}/dev/null" c 1 3

rm -f "${ROOTFS_IMG}"
(
    cd "${ROOTFS}"
    find . | cpio -o -H newc | gzip -9 > "${ROOTFS_IMG}"
)

log "day24 initramfs 构建完成：${ROOTFS_IMG}"
