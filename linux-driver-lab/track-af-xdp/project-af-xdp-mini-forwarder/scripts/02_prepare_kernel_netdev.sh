#!/usr/bin/env bash
#============================================================
# 02_prepare_kernel_netdev.sh — 确保实验网卡可用
#
# 功能：
#   1. 检查实验网卡是否存在
#   2. 可选：重新绑定驱动（若 DPDK 占用了 PCI）
#   3. 清理已有的 XDP attach 状态
#   4. 确保网卡 up
#
# 使用：
#   sudo ./scripts/02_prepare_kernel_netdev.sh
#   sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh  # 带 rebind
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "prepare kernel netdev"

record_dir="$(latest_record_dir)"
out="${record_dir}/PREPARE_KERNEL_NETDEV.txt"

{
    write_env_header
    echo

    echo "== before =="
    ip link show "${AF_XDP_IFACE}" || true
    ethtool -i "${AF_XDP_IFACE}" || true
    echo

    if [[ "${AF_XDP_CONFIRM_REBIND:-NO}" != "YES" ]]; then
        echo "SKIP_REBIND: set AF_XDP_CONFIRM_REBIND=YES to allow driver check/rebind"
    else
        echo "REBIND_ALLOWED=YES"
        modprobe "${AF_XDP_DRIVER}" || true
        ip link set "${AF_XDP_IFACE}" up || true
    fi
    echo

    echo "== detach existing xdp best effort =="
    # 尝试清理所有类型的 XDP attach（通用/驱动）
    ip link set dev "${AF_XDP_IFACE}" xdp off 2>/dev/null || true
    ip link set dev "${AF_XDP_IFACE}" xdpgeneric off 2>/dev/null || true
    echo

    echo "== after =="
    ip link show "${AF_XDP_IFACE}" || true
    ethtool -i "${AF_XDP_IFACE}" || true
    ip -details link show "${AF_XDP_IFACE}" || true
} 2>&1 | tee "${out}"