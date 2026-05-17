#!/usr/bin/env bash
#============================================================
# 08_collect_stats.sh — 收集网卡的统计信息
#
# 功能：
#   1. 收集网卡的链路层统计（ip -s link）
#   2. 收集网卡的详细链路信息（ip -d link）
#   3. 收集网卡驱动的详细统计（ethtool -S）
#   4. 读取 /proc/softirqs 确认软中断分布
#   5. 列出本次记录目录中已生成的所有日志文件
#
# 输出：
#   records/.../COLLECT_STATS.txt
#
# 使用：
#   ./scripts/08_collect_stats.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/COLLECT_STATS.txt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Is)"
    echo "KERNEL=$(uname -r)"
    echo "BPFTRACE_IFACE=${BPFTRACE_IFACE}"
    echo
    echo "== ip -s link =="
    # 网卡的基本统计：收发包数、错误、丢包等
    ip -s link show "${BPFTRACE_IFACE}" 2>&1 || true
    echo
    echo "== ip -d link =="
    # 显示详细链路信息（包含 xdp 状态）
    ip -d link show "${BPFTRACE_IFACE}" 2>&1 || true
    echo
    echo "== ethtool -S =="
    # 网卡驱动的详细统计（驱动特定）
    ethtool -S "${BPFTRACE_IFACE}" 2>&1 | head -160 || true
    echo
    echo "== softirqs =="
    # /proc/softirqs 显示各 CPU 核心的软中断处理统计
    grep -E 'NET_RX|NET_TX' /proc/softirqs || true
    echo
    echo "== generated logs =="
    # 列出本次记录目录中所有的日志文件
    find "${OUT_DIR}" -maxdepth 1 -type f -printf '%f\n' | sort
} | tee "${OUT}"

echo "COLLECT_STATS=${OUT}"