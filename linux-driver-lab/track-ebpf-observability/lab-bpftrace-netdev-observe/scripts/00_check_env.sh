#!/usr/bin/env bash
#============================================================
# 00_check_env.sh — 检查 bpftrace 观测实验的运行环境
#
# 功能：
#   1. 输出实验环境元信息（LAB名、日期、内核版本、主机名、用户）
#   2. 检查必要工具是否存在（bpftrace/bpftool/ip/ethtool/timeout）
#   3. 显示目标网卡的链路层详细信息（ip -d link）
#   4. 检查是否有 XDP 程序附加（可能干扰 tracepoint）
#   5. 显示管理网口的状态
#   6. 检查 tracefs/debugfs 是否挂载
#   7. 列出当前内核支持的所有 net:* tracepoint
#
# 输出：
#   records/YYYYMMDD-HHMMSS-bpftrace-netdev-observe/ENV_CHECK.txt
#
# 使用：
#   ./scripts/00_check_env.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 创建带时间戳的记录目录
OUT_DIR="$(new_record_dir)"
OUT="${OUT_DIR}/ENV_CHECK.txt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Is)"
    echo "KERNEL=$(uname -r)"
    echo "HOSTNAME=$(hostname)"
    echo "USER=$(id)"
    echo "BPFTRACE_IFACE=${BPFTRACE_IFACE}"
    echo "BPFTRACE_MANAGEMENT_IFACE=${BPFTRACE_MANAGEMENT_IFACE}"
    echo "BPFTRACE_DURATION=${BPFTRACE_DURATION}"
    echo
    echo "== tools =="
    # 检查每个必要工具是否安装，输出路径和版本信息
    for t in bpftrace bpftool ip ethtool timeout; do
        if command -v "$t" >/dev/null 2>&1; then
            echo "$t: $(command -v "$t")"
            "$t" --version 2>/dev/null | head -5 || true
        else
            echo "$t: NOT_FOUND"
        fi
        echo
    done
    echo "== iface =="
    # 显示实验网卡的链路信息（简洁版）
    ip -br link show "${BPFTRACE_IFACE}" 2>&1 || true
    # 显示详细链路信息（包含 xdp 状态）
    ip -d link show "${BPFTRACE_IFACE}" 2>&1 || true
    # 显示网卡统计（收发包数、错误等）
    ip -s link show "${BPFTRACE_IFACE}" 2>&1 || true
    # 显示网卡驱动信息
    ethtool -i "${BPFTRACE_IFACE}" 2>&1 || true
    echo
    echo "== xdp status =="
    # 检查是否有 XDP 程序已附加到目标网卡
    # 如果有，skb 级别的 tracepoint 可能看不到数据包
    if has_xdp_attached; then
        echo "XDP_ATTACHED=YES"
        echo "WARN: existing XDP program may bypass skb tracepoints. Run: sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh"
    else
        echo "XDP_ATTACHED=NO"
    fi
    echo
    echo "== management iface =="
    # 显示管理网口状态（仅作参考，观测不在此进行）
    ip -br link show "${BPFTRACE_MANAGEMENT_IFACE}" 2>&1 || true
    echo
    echo "== tracefs/debugfs =="
    # 检查 bpftrace 依赖的 tracefs 是否可用
    mount | grep -E 'tracefs|debugfs|bpf' || true
    ls -rd /sys/kernel/tracing /sys/kernel/debug/tracing 2>&1 || true
    echo
    echo "== available net tracepoints =="
    # 列出当前内核支持的所有 net:* tracepoint
    # 这是主要的观测点来源
    bpftrace -l 'tracepoint:net:*' 2>&1 | head -80 || true
} | tee "${OUT}"

echo "ENV_CHECK=${OUT}"