#!/usr/bin/env bash
# 脚本: 02_bind_vmxnet3.sh
# 功能: 将 vmxnet3 网卡绑定到 vfio-pci 驱动（DPDK 使用），或恢复为内核驱动
# 用法:
#   ./scripts/02_bind_vmxnet3.sh status    # 查看绑定状态
#   sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind   # 绑定到 vfio-pci
#   sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh unbind # 恢复为 vmxnet3

set -euo pipefail
# 加载公共函数库
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 默认动作为 status（查看绑定状态）
ACTION="${1:-status}"
RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

# 防止误将管理网卡绑定到 DPDK
guard_not_mgmt_pci

devbind="$(find_devbind || true)"
if [[ -z "${devbind}" ]]; then
    echo "ERROR: dpdk-devbind.py not found. Install dpdk tools or set DPDK_DEVBIND=/path/to/dpdk-devbind.py" >&2
    exit 1
fi

# 打印当前 DPDK 设备绑定状态
status() {
    "${devbind}" --status
    echo
    echo "Target:"
    echo "  DPDK_IF=${DPDK_IF}"
    echo "  DPDK_PCI=${DPDK_PCI}"
    echo "  DPDK_DRIVER=${DPDK_DRIVER}"
    echo "  MGMT_IF=${MGMT_IF}"
}

case "${ACTION}" in
    status)
        # 查看 DPDK 设备绑定状态
        OUT="${RECORD_DIR}/BIND_STATUS.txt"
        {
            echo "# BIND_STATUS"
            echo "date=$(date '+%F %T')"
            echo
            status
        } > "${OUT}" 2>&1
        cat "${OUT}"
        echo
        echo "[OK] Bind status saved: ${OUT}"
        ;;

    bind)
        # 将网卡绑定到 vfio-pci 驱动（DPDK 使用）
        require_root_for_write
        if [[ "${DPDK_CONFIRM_BIND:-}" != "YES" ]]; then
            cat >&2 <<EOF
ERROR: bind will detach ${DPDK_IF}/${DPDK_PCI} from Linux kernel networking.
Please rerun explicitly:

  sudo DPDK_CONFIRM_BIND=YES $0 bind
EOF
            exit 2
        fi

        OUT_BEFORE="${RECORD_DIR}/BIND_BEFORE.txt"
        OUT_AFTER="${RECORD_DIR}/BIND_AFTER.txt"

        # 记录绑定前的状态：绑定信息、IP 地址、驱动信息
        {
            echo "# BIND_BEFORE"
            echo "date=$(date '+%F %T')"
            echo
            status
            echo
            ip -br addr || true
            ethtool -i "${DPDK_IF}" || true
        } > "${OUT_BEFORE}" 2>&1

        # 加载 vfio 内核模块（IOMMU 隔离需要）
        modprobe vfio || true
        modprobe vfio-pci || true

        # 将网卡 down 后再绑定（防止内核报错）
        if ip link show "${DPDK_IF}" >/dev/null 2>&1; then
            ip link set "${DPDK_IF}" down || true
        fi

        # 执行绑定：将 PCI 设备绑定到 vfio-pci 驱动
        "${devbind}" -b "${DPDK_DRIVER}" "${DPDK_PCI}" >> "${OUT_AFTER}" 2>&1 || {
            rc=$?
            echo "ERROR: dpdk-devbind failed, rc=${rc}. See ${OUT_AFTER}" >&2
            exit "${rc}"
        }

        # 记录绑定后的状态
        {
            echo
            echo "# BIND_AFTER"
            echo "date=$(date '+%F %T')"
            echo
            status
        } >> "${OUT_AFTER}" 2>&1

        echo "[OK] Bind finished:"
        echo "  before: ${OUT_BEFORE}"
        echo "  after : ${OUT_AFTER}"
        ;;

    unbind|restore)
        # 解除绑定，恢复为内核 vmxnet3 驱动
        require_root_for_write
        if [[ "${DPDK_CONFIRM_BIND:-}" != "YES" ]]; then
            cat >&2 <<EOF
ERROR: restore will rebind ${DPDK_PCI} to kernel vmxnet3.
Please rerun explicitly:

  sudo DPDK_CONFIRM_BIND=YES $0 unbind
EOF
            exit 2
        fi

        OUT="${RECORD_DIR}/UNBIND_TO_KERNEL.txt"
        : > "${OUT}"

        {
            echo "# UNBIND_TO_KERNEL"
            echo "date=$(date '+%F %T')"
            echo
            echo "## Before"
            status
            echo
        } >> "${OUT}" 2>&1

        # 加载 vmxnet3 内核模块并重新绑定
        modprobe vmxnet3 || true
        "${devbind}" -b vmxnet3 "${DPDK_PCI}" >> "${OUT}" 2>&1 || true

        # 恢复后：重新 up 网卡、显示 IP 和驱动信息
        {
            echo
            echo "## After"
            status
            echo
            ip link set "${DPDK_IF}" up 2>/dev/null || true
            ip -br addr || true
            ethtool -i "${DPDK_IF}" 2>/dev/null || true
        } >> "${OUT}" 2>&1

        echo "[OK] Restore attempt saved: ${OUT}"
        ;;

    *)
        cat >&2 <<EOF
Usage:
  $0 status
  sudo DPDK_CONFIRM_BIND=YES $0 bind
  sudo DPDK_CONFIRM_BIND=YES $0 unbind

Environment:
  DPDK_IF=${DPDK_IF}
  DPDK_PCI=${DPDK_PCI}
  DPDK_DRIVER=${DPDK_DRIVER}
  MGMT_IF=${MGMT_IF}
EOF
        exit 1
        ;;
esac
