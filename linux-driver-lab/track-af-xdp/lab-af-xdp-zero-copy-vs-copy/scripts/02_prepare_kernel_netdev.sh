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
#   必须设置 AF_XDP_CONFIRM_REBIND=YES。
#
# 示例：
#   sudo AF_XDP_CONFIRM_REBIND=YES ./scripts/02_prepare_kernel_netdev.sh
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
        # 未确认绑定，跳过 rebind 操作（仅检查状态）
        echo "SKIP_REBIND: set AF_XDP_CONFIRM_REBIND=YES to allow driver check/rebind"
    else
        echo "REBIND_ALLOWED=YES"
        # 显示 DPDK 当前绑定状态
        if command -v dpdk-devbind.py >/dev/null 2>&1; then
            dpdk-devbind.py --status | sed -n '1,120p' || true
        fi
        # 加载驱动并 up 网卡
        modprobe "${AF_XDP_DRIVER}" || true
        ip link set "${AF_XDP_IFACE}" up || true
    fi
    echo

    echo "== after =="
    ip link show "${AF_XDP_IFACE}" || true
    ethtool -i "${AF_XDP_IFACE}" || true
} 2>&1 | tee "${out}"