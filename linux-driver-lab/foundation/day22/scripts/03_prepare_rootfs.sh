#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

log "开始构建 day22 独立 initramfs"
auto_fill_platform_paths
require_file_named "BUSYBOX_BIN" "${BUSYBOX_BIN}"

# 先构建 day22 自己的 guest 侧 C 工具。
"${SCRIPT_DIR}/02_build_guest_tools.sh"
require_executable_named "GUEST_PCI_SYSFS_DUMP_BIN" "${GUEST_PCI_SYSFS_DUMP_BIN}"

# 如果当前 shell 没有显式 export GUEST_LSPCI_BIN，但 pciutils 源码目录里已经有编好的 lspci，
# 就直接使用它，避免把“已编好的第三方工具”又绕丢。
if [[ ! -x "${GUEST_LSPCI_BIN}" && -n "${PCIUTILS_SRC_DIR:-}" && -x "${PCIUTILS_SRC_DIR}/lspci" ]]; then
    export GUEST_LSPCI_BIN="${PCIUTILS_SRC_DIR}/lspci"
    log "复用已存在的第三方 arm64 lspci：${GUEST_LSPCI_BIN}"
fi

# 如果没有 guest lspci，就尝试自动构建一次。
if [[ ! -x "${GUEST_LSPCI_BIN}" ]]; then
    warn "未发现 guest lspci，尝试自动构建。"
    if [[ ! -d "${PCIUTILS_SRC_DIR}" ]]; then
        warn "当前包默认不会内置完整 pciutils 源码目录。"
        warn "如果你之前已经在别的 day22 目录编过 lspci，请重新 export GUEST_LSPCI_BIN=/path/to/lspci。"
        warn "否则请重新 git clone pciutils 到 ${PCIUTILS_SRC_DIR}，或执行 make discover-paths 查看推荐路径。"
    fi
    "${SCRIPT_DIR}/02_build_guest_lspci.sh"
fi
require_executable_named "GUEST_LSPCI_BIN" "${GUEST_LSPCI_BIN}"

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
    find . -print0 | sort -z | ${CPIO_BIN} --null -o -H newc | ${GZIP_BIN} -9 > "${ROOTFS_IMAGE}"
)

log "day22 initramfs 构建完成：${ROOTFS_IMAGE}"
