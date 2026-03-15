#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

log "开始检查 day22 宿主机环境"

auto_required=(bash mkdir cp find ${CPIO_BIN} ${GZIP_BIN} ${TIMEOUT_BIN} ${AWK_BIN} ${GREP_BIN} ${SED_BIN} ${FILE_BIN})
for cmd in "${auto_required[@]}"; do
    require_cmd "$cmd"
done

# QEMU 与 ivshmem-server 是 day22 运行主链路必须工具。
require_cmd "${QEMU_BIN}"
require_cmd "${IVSHMEM_SERVER_BIN}"

print_kv "DAY22_ROOT" "${DAY22_ROOT}"
print_kv "WORKDIR" "${WORKDIR}"
print_kv "QEMU_BIN" "${QEMU_BIN}"
print_kv "IVSHMEM_SERVER_BIN" "${IVSHMEM_SERVER_BIN}"
print_kv "KERNEL_IMAGE" "${KERNEL_IMAGE:-<未设置>}"
print_kv "BUSYBOX_BIN" "${BUSYBOX_BIN:-<未设置>}"
print_kv "GUEST_LSPCI_BIN" "${GUEST_LSPCI_BIN:-<未设置>}"
print_kv "GUEST_PCI_SYSFS_DUMP_BIN" "${GUEST_PCI_SYSFS_DUMP_BIN:-<未设置>}"
print_kv "KERNEL_CONFIG_PATH" "${KERNEL_CONFIG_PATH:-<未设置>}"

# 内核镜像与 BusyBox 是构建最小 guest 的硬条件。
[[ -n "${KERNEL_IMAGE}" ]] || die "请先 export KERNEL_IMAGE=/path/to/Image"
[[ -n "${BUSYBOX_BIN}" ]] || die "请先 export BUSYBOX_BIN=/path/to/busybox"
require_file "${KERNEL_IMAGE}"
require_file "${BUSYBOX_BIN}"

# lspci 是 day22 的主验收项，所以要么已有二进制，要么要准备 pciutils 源码构建。
if [[ -x "${GUEST_LSPCI_BIN}" ]]; then
    if is_elf_aarch64_static "${GUEST_LSPCI_BIN}"; then
        log "发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
    else
        warn "${GUEST_LSPCI_BIN} 存在，但看起来不是 arm64 静态 ELF。"
        warn "如果 guest 是最小 initramfs，强烈建议提供 arm64 静态 lspci。"
    fi
else
    warn "当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN}"
    warn "后续会尝试从 PCIUTILS_SRC_DIR 构建；若源码也没有，则 day22 无法完成 lspci 验收。"
fi

if [[ -n "${KERNEL_CONFIG_PATH}" ]]; then
    if [[ -f "${KERNEL_CONFIG_PATH}" ]]; then
        log "内核配置文件已指定：${KERNEL_CONFIG_PATH}"
    else
        warn "KERNEL_CONFIG_PATH 已设置，但文件不存在：${KERNEL_CONFIG_PATH}"
    fi
else
    warn "未设置 KERNEL_CONFIG_PATH；day22 仍可继续，但内核 PCI 配置检查会跳过。"
fi

log "宿主机基础检查完成"
