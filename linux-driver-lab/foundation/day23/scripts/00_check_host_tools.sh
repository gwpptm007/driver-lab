#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

log "开始检查 day23 宿主机环境"
require_cmd "${QEMU_BIN}"
require_cmd "${CROSS_COMPILE}gcc"
require_cmd "${CROSS_COMPILE}strip"
require_cmd find
require_cmd cpio
require_cmd gzip

require_file KERNEL_IMAGE "${KERNEL_IMAGE}"
require_file BUSYBOX_BIN "${BUSYBOX_BIN}"
require_file KDIR "${KDIR}"

cat <<EOF
DAY23_ROOT               : ${DAY23_ROOT}
WORKDIR                  : ${WORKDIR}
QEMU_BIN                 : ${QEMU_BIN}
ARCH                     : ${ARCH}
CROSS_COMPILE            : ${CROSS_COMPILE}
KERNEL_SRC_ROOT          : ${KERNEL_SRC_ROOT}
KDIR                     : ${KDIR}
KERNEL_IMAGE             : ${KERNEL_IMAGE}
BUSYBOX_BIN              : ${BUSYBOX_BIN}
GUEST_LSPCI_BIN          : ${GUEST_LSPCI_BIN}
MODULE_FILE              : ${MODULE_FILE}
IVSHMEM_SIZE             : ${IVSHMEM_SIZE}
IVSHMEM_DEVICE_ID_EXPECT : ${IVSHMEM_DEVICE_ID_EXPECT}
EOF

if [[ -x "${GUEST_LSPCI_BIN}" ]]; then
    log "发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
else
    warn "当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN}"
    warn "可执行 make build-lspci，或手工准备第三方源码到 ${PCIUTILS_SRC_DIR}"
fi
