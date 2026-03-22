#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day26] 开始构建 day26 独立 initramfs"
# 统一用 bash 调子脚本，避免 zip 解压后执行位丢失导致 Permission denied。
bash "${DAY26_ROOT}/scripts/03_build_tools.sh"

# rootfs 里要打入 day26_edu_tool.ko，因此如果模块尚未生成，这里自动补一轮。
if [ ! -f "${DAY26_ROOT}/driver/day26_edu_tool.ko" ]; then
    echo "[day26][WARN] 未发现 day26_edu_tool.ko，先自动构建模块。"
    bash "${DAY26_ROOT}/scripts/09_build_day26_module.sh"
fi

# lspci 是 guest 自动流程的前置。如果当前目录还没准备好，就自动构建。
if [ ! -x "${GUEST_LSPCI_BIN:-}" ]; then
    echo "[day26][WARN] 未发现 guest lspci，尝试自动构建。"
    bash "${DAY26_ROOT}/scripts/02_build_guest_lspci.sh"
fi

require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
require_exec "${WORKDIR}/tools/aarch64/day26_edu_tool" GUEST_TOOL_BIN
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
require_file "${DAY26_ROOT}/driver/day26_edu_tool.ko" "day26 module"

rm -rf "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}"/{bin,dev,etc,proc,sys,tmp,root,lib,usr/bin}

cp -f "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
chmod +x "${ROOTFS_DIR}/bin/busybox"
for app in sh mount umount mkdir ls cat echo dmesg grep cut sleep poweroff insmod rmmod mknod chmod chown head tr uname; do
    ln -sf /bin/busybox "${ROOTFS_DIR}/bin/${app}"
done

cp -f "${GUEST_LSPCI_BIN}" "${ROOTFS_DIR}/bin/lspci"
chmod +x "${ROOTFS_DIR}/bin/lspci"
cp -f "${WORKDIR}/tools/aarch64/day26_edu_tool" "${ROOTFS_DIR}/bin/day26_edu_tool"
chmod +x "${ROOTFS_DIR}/bin/day26_edu_tool"
cp -f "${DAY26_ROOT}/driver/day26_edu_tool.ko" "${ROOTFS_DIR}/root/day26_edu_tool.ko"
chmod 0644 "${ROOTFS_DIR}/root/day26_edu_tool.ko"
cp -f "${DAY26_ROOT}/guest/init.day26" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

# 这里需要 CAP_MKNOD，所以通常必须 sudo -E make rootfs。
mknod -m 600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 666 "${ROOTFS_DIR}/dev/null" c 1 3

(
    cd "${ROOTFS_DIR}"
    find . -print0 | cpio --null -ov --format=newc 2>/dev/null | gzip -9 > "${ROOTFS_IMG}"
)

echo "[day26] day26 initramfs 构建完成：${ROOTFS_IMG}"
