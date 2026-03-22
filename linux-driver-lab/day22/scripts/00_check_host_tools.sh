#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

log "开始检查 day22 宿主机环境（ivshmem-plain 版本）"
auto_fill_platform_paths

auto_required=(bash mkdir cp find ${CPIO_BIN} ${GZIP_BIN} ${TIMEOUT_BIN} ${AWK_BIN} ${GREP_BIN} ${SED_BIN} ${FILE_BIN} ${TRUNCATE_BIN})
for cmd in "${auto_required[@]}"; do
    require_cmd "$cmd"
done

require_cmd "${QEMU_BIN}"

print_kv "DAY22_ROOT" "${DAY22_ROOT}"
print_kv "WORKDIR" "${WORKDIR}"
print_kv "QEMU_BIN" "${QEMU_BIN}"
print_kv "KERNEL_IMAGE" "${KERNEL_IMAGE:-<未设置>}"
print_kv "BUSYBOX_BIN" "${BUSYBOX_BIN:-<未设置>}"
print_kv "GUEST_LSPCI_BIN" "${GUEST_LSPCI_BIN:-<未设置>}"
print_kv "GUEST_PCI_SYSFS_DUMP_BIN" "${GUEST_PCI_SYSFS_DUMP_BIN:-<未设置>}"
print_kv "KERNEL_CONFIG_PATH" "${KERNEL_CONFIG_PATH:-<未设置>}"
print_kv "IVSHMEM_SIZE" "${IVSHMEM_SIZE}"
print_kv "IVSHMEM_DEVICE_ID_EXPECT" "${IVSHMEM_DEVICE_ID_EXPECT}"

if [[ -z "${KERNEL_IMAGE}" || -z "${BUSYBOX_BIN}" ]]; then
    warn "你现在还没有把真实路径 export 进当前 shell。"
    warn "先执行：make discover-paths"
fi

[[ -n "${KERNEL_IMAGE}" ]] || die "请先 export KERNEL_IMAGE=/path/to/Image（可先执行 make discover-paths）"
[[ -n "${BUSYBOX_BIN}" ]] || die "请先 export BUSYBOX_BIN=/path/to/busybox（可先执行 make discover-paths）"
require_file "${KERNEL_IMAGE}"
require_executable_file "${BUSYBOX_BIN}"

if ! ${FILE_BIN} "${KERNEL_IMAGE}" | ${GREP_BIN} -qi "ARM aarch64\|ARM64"; then
    warn "KERNEL_IMAGE 看起来不是 arm64 镜像：${KERNEL_IMAGE}"
fi
if ! ${FILE_BIN} "${BUSYBOX_BIN}" | ${GREP_BIN} -qi "ARM aarch64\|ARM64"; then
    warn "BUSYBOX_BIN 看起来不是 arm64 BusyBox：${BUSYBOX_BIN}"
fi

if [[ -x "${GUEST_LSPCI_BIN}" ]]; then
    if is_elf_aarch64_static "${GUEST_LSPCI_BIN}"; then
        log "发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
    else
        warn "${GUEST_LSPCI_BIN} 存在，但看起来不是 arm64 静态 ELF。"
        warn "最小 initramfs 环境强烈建议使用 arm64 静态 lspci。"
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

log "宿主机基础检查完成；D22 默认不再要求 ivshmem-server。"
