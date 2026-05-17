#!/usr/bin/env bash
#============================================================
# 02_clean_xdp_if_attached.sh — 清理目标网卡上的 XDP 程序
#
# 功能：
#   1. 检查 BPFTRACE_IFACE 指定的网卡是否有 XDP 程序附加
#   2. 如果有，需要确认（EBPF_CONFIRM_XDP_OFF=YES）才执行 detach
#   3. 记录操作前后状态到日志
#
# 为什么需要：
#   如果目标网卡有 XDP 程序附加，skb 级别的 tracepoint 可能看不到数据包
#   因为 XDP 在更早期处理数据包，绕过了部分网络栈
#
# 环境变量：
#   EBPF_CONFIRM_XDP_OFF=YES  — 确认关闭 XDP（安全保护）
#
# 输出：
#   records/.../XDP_CLEAN.txt
#
# 使用：
#   sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/XDP_CLEAN.txt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Is)"
    echo "IFACE=${BPFTRACE_IFACE}"
    echo
    echo "== before =="
    # 显示附加前的网卡详细状态
    ip -d link show "${BPFTRACE_IFACE}" 2>&1 || true
    echo
    if has_xdp_attached; then
        echo "XDP_ATTACHED_BEFORE=YES"
        # 只有明确设置 EBPF_CONFIRM_XDP_OFF=YES 才执行 detach
        if [[ "${EBPF_CONFIRM_XDP_OFF:-NO}" == "YES" ]]; then
            echo "ACTION=ip link set dev ${BPFTRACE_IFACE} xdp off"
            ip link set dev "${BPFTRACE_IFACE}" xdp off
            echo "XDP_CLEAN_STATUS=CLEANED"
        else
            echo "XDP_CLEAN_STATUS=WARN_ATTACHED_NOT_CLEANED"
            echo "Set EBPF_CONFIRM_XDP_OFF=YES to detach existing XDP program."
        fi
    else
        echo "XDP_ATTACHED_BEFORE=NO"
        echo "XDP_CLEAN_STATUS=ALREADY_OFF"
    fi
    echo
    echo "== after =="
    ip -d link show "${BPFTRACE_IFACE}" 2>&1 || true
} | tee "${OUT}"

echo "XDP_CLEAN=${OUT}"