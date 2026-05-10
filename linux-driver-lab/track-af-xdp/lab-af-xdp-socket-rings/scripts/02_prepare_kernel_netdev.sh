#!/usr/bin/env bash
#============================================================
# 02_prepare_kernel_netdev.sh — 确保实验网卡可用
#
# 功能：
#   1. 检查实验网卡是否存在
#   2. 若不存在（可能是 DPDK 绑定到了 vfio-uio），尝试重新绑定驱动
#   3. 将网卡 up，为 AF_XDP 实验做准备
#
# 使用：
#   sudo ./scripts/02_prepare_kernel_netdev.sh
#
# 重新绑定条件：
#   必须设置 AF_XDP_CONFIRM_REBIND=YES，否则脚本会拒绝操作。
#   这是防止误操作导致管理网口断线的保护措施。
#
# 示例：
#   sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（PCI 绑定、网卡 up）
require_root

# 拒绝在管理网口上操作（防止 SSH 断开）
refuse_management_iface "prepare AF_XDP kernel netdev"

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/PREPARE_KERNEL_NETDEV.txt"

{
    write_env_header
    echo

    # ---- 绑定前的状态 ----
    echo "== before =="
    ip -br link show "${AF_XDP_IFACE}" 2>/dev/null || echo "iface ${AF_XDP_IFACE}: NOT_FOUND"
    lspci -nnk -s "${AF_XDP_PCI}" || true
    echo

    # ---- 检查或重建网卡 ----
    if [[ -d "/sys/class/net/${AF_XDP_IFACE}" ]]; then
        # 网卡已存在，不需要重新绑定
        echo "iface ${AF_XDP_IFACE} already exists."
    else
        # 网卡不存在（DPDK 占用），需要重新绑定
        if [[ "${AF_XDP_CONFIRM_REBIND:-NO}" != "YES" ]]; then
            echo "ERROR: iface ${AF_XDP_IFACE} not found."
            echo "To rebind ${AF_XDP_PCI} to ${AF_XDP_DRIVER}, run:"
            echo "  sudo AF_XDP_CONFIRM_REBIND=YES $0"
            exit 2
        fi
        # 加载驱动
        modprobe "${AF_XDP_DRIVER}" || true
        # 使用 dpdk-devbind.py 重新绑定
        if command -v dpdk-devbind.py >/dev/null 2>&1; then
            dpdk-devbind.py -b "${AF_XDP_DRIVER}" "${AF_XDP_PCI}"
        else
            echo "ERROR: dpdk-devbind.py not found; manual rebind required."
            exit 3
        fi
        # 等待网卡枚举
        sleep 2
    fi

    # 确保网卡处于 up 状态（AF_XDP 需要）
    ip link set "${AF_XDP_IFACE}" up || true

    # ---- 绑定后的状态 ----
    echo
    echo "== after =="
    ip -br link show "${AF_XDP_IFACE}" 2>/dev/null || true
    ethtool -i "${AF_XDP_IFACE}" 2>/dev/null || true
    lspci -nnk -s "${AF_XDP_PCI}" || true
    echo

    echo "PREPARE_RESULT=PASS_OR_MANUAL_CHECK"
} 2>&1 | tee "${OUT}"