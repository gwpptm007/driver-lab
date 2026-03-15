#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

log "开始构建 day22 独立 initramfs"
require_file "${BUSYBOX_BIN}"

# 先构建 day22 自己的 guest 侧 C 工具。
"${SCRIPT_DIR}/02_build_guest_tools.sh"
require_executable_file "${GUEST_PCI_SYSFS_DUMP_BIN}"

# 如果没有 guest lspci，就尝试自动构建一次。
if [[ ! -x "${GUEST_LSPCI_BIN}" ]]; then
    warn "未发现 guest lspci，尝试自动构建。"
    "${SCRIPT_DIR}/02_build_guest_lspci.sh"
fi
require_executable_file "${GUEST_LSPCI_BIN}"

ensure_dir "${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}"/{bin,sbin,etc,proc,sys,dev,tmp,run,root,mnt}

# 拷入 BusyBox，并创建常用 applet 软链接。
cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
chmod +x "${ROOTFS_DIR}/bin/busybox"
for app in sh mount umount mkdir mknod dmesg grep cat ls sleep echo uname poweroff hexdump sync chmod; do
    ln -sf /bin/busybox "${ROOTFS_DIR}/bin/${app}"
done
ln -sf /bin/busybox "${ROOTFS_DIR}/sbin/init"

# 拷入 guest lspci 与 day22 自己的 C 枚举工具。
cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
chmod +x "${ROOTFS_DIR}/bin/lspci"
cp -f "${GUEST_PCI_SYSFS_DUMP_BIN}" "${ROOTFS_DIR}/bin/pci_sysfs_dump"
chmod +x "${ROOTFS_DIR}/bin/pci_sysfs_dump"

# 生成 init 脚本。这里不直接复制前面 day 的 init，而是用 day22 自己的模板。
sed \
    -e "s#__DAY22_RUN_ID__#${RUN_ID}#g" \
    -e "s#__DAY22_DEVICE_FILTER__#1af4:1110#g" \
    "${DAY22_ROOT}/guest/init.day22" > "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

# 生成最小设备节点。
mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3

# 打包为 initramfs。
rm -f "${ROOTFS_IMAGE}"
(
    cd "${ROOTFS_DIR}"
    find . -print0 | sort -z | xargs -0 ${CPIO_BIN} -o -H newc | ${GZIP_BIN} -9 > "${ROOTFS_IMAGE}"
)

log "day22 initramfs 构建完成：${ROOTFS_IMAGE}"
