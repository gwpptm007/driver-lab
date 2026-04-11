#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

log "扫描当前仓库常见的 arm64 平台产物路径"

image_candidate="$(find "${REPO_ROOT}/../kernel-src" -path '*/build/arm64/arch/arm64/boot/Image' 2>/dev/null | head -n1 || true)"
config_candidate="$(find "${REPO_ROOT}/../kernel-src" -path '*/build/arm64/.config' 2>/dev/null | head -n1 || true)"
busybox_candidate="$(find "${REPO_ROOT}/../kernel-src" -path '*/busybox-1.36.1/build/arm64/busybox' 2>/dev/null | head -n1 || true)"
lspci_candidate="$(find "${DAY23_ROOT}" -path '*/pciutils/lspci' -type f 2>/dev/null | head -n1 || true)"
kdir_candidate="$(find "${REPO_ROOT}/../kernel-src" -path '*/build/arm64' -type d 2>/dev/null | head -n1 || true)"
ksrc_candidate="$(find "${REPO_ROOT}/../kernel-src" -path '*/src/scripts/config' -type f 2>/dev/null | sed 's#/scripts/config$##' | head -n1 || true)"

cat <<EOF
REPO_ROOT                : ${REPO_ROOT}
Image 候选             : ${image_candidate:-<未找到>}
.config 候选           : ${config_candidate:-<未找到>}
BusyBox 候选           : ${busybox_candidate:-<未找到>}
lspci 候选             : ${lspci_candidate:-<未找到>}
KDIR 候选              : ${kdir_candidate:-<未找到>}
KERNEL_SRC_ROOT 候选   : ${ksrc_candidate:-<未找到>}

# 复制下面这组命令到当前 shell：
export RUN_ID=day23-local-001
export KERNEL_SRC_ROOT=${ksrc_candidate:-/请手动填写/kernel/src}
export KDIR=${kdir_candidate:-/请手动填写/kernel/build/arm64}
export KERNEL_IMAGE=${image_candidate:-/请手动填写/Image}
export KERNEL_CONFIG_PATH=${config_candidate:-/请手动填写/.config}
export BUSYBOX_BIN=${busybox_candidate:-/请手动填写/busybox}
export PCIUTILS_SRC_DIR=${DAY23_ROOT}/third_party/pciutils
export GUEST_LSPCI_BIN=${lspci_candidate:-/请手动填写/lspci}
EOF
