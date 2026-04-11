#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 运行前的统一环境检查：
# - QEMU / 内核镜像 / BusyBox 是否可用
# - GUEST_LSPCI_BIN 是否已经准备好
# - （可选）检查 .config 里 PCI/MSI 关键开关
ensure_dir "$(run_dir)"

echo "[day25] 开始检查 day25 宿主机环境（EDU + MSI 版本）"
echo "DAY25_ROOT              : ${DAY25_ROOT}"
echo "WORKDIR                 : ${WORKDIR}"
echo "QEMU_BIN                : ${QEMU_BIN}"
echo "KERNEL_IMAGE            : ${KERNEL_IMAGE:-}"
echo "BUSYBOX_BIN             : ${BUSYBOX_BIN:-}"
echo "GUEST_LSPCI_BIN         : ${GUEST_LSPCI_BIN:-}"
echo "KERNEL_CONFIG_PATH      : ${KERNEL_CONFIG_PATH:-}"
echo "KDIR                    : ${KDIR:-}"
echo "EDU_DEVICE_ID_EXPECT    : ${EDU_DEVICE_ID_EXPECT}"

command -v "${QEMU_BIN}" >/dev/null 2>&1 || { echo "[day25][ERROR] 找不到 ${QEMU_BIN}"; exit 1; }
require_file "${KERNEL_IMAGE:-}" KERNEL_IMAGE
require_exec "${BUSYBOX_BIN:-}" BUSYBOX_BIN

if [ -x "${GUEST_LSPCI_BIN:-}" ]; then
    echo "[day25] 发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
else
    echo "[day25][WARN] 当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN:-}"
    echo "[day25][WARN] 请先按 README / START_HERE 的主流程执行 git clone + make build-lspci"
fi

if [ -n "${KERNEL_CONFIG_PATH:-}" ] && [ -f "${KERNEL_CONFIG_PATH}" ]; then
    echo "[day25] 内核配置文件已指定：${KERNEL_CONFIG_PATH}"
    {
        echo "# day25 kernel config check"
        echo "# file: ${KERNEL_CONFIG_PATH}"
        echo
        for sym in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCI_HOST_GENERIC CONFIG_VFIO; do
            if grep -q "^${sym}=y" "${KERNEL_CONFIG_PATH}"; then
                echo "[OK] ${sym}=y"
            else
                echo "[WARN] ${sym} not set to y"
            fi
        done
    } | tee "$(run_dir)/kernel-config-check.txt"
else
    echo "[day25][WARN] 未设置 KERNEL_CONFIG_PATH，跳过内核 PCI 配置检查。"
fi
