#!/usr/bin/env bash
#============================================================
# 03_run_rx.sh — 观测 RX（接收）路径的 tracepoint
#
# 功能：
#   运行 bpftrace 脚本观测网络卡接收路径：
#   NIC RX → netif_receive_skb tracepoint → 网络协议栈
#
# 探针：
#   tracepoint:net:netif_receive_skb
#   （推荐使用 tracepoint，比 kprobe 更稳定，不受 BTF 影响）
#
# 统计：
#   - @rx_total: 总收包数
#   - @rx_cpu[cpu]: 每个 CPU 核心的收包数
#
# 依赖：
#   - BPFTRACE_IFACE 指定的网卡有真实流量经过
#   - 无流量时计数器为零（不代表脚本错误）
#   - XDP 程序可能导致 skb tracepoint 看不到数据包
#
# 输出：
#   records/.../RX_TRACEPOINT.log
#
# 使用：
#   sudo ./scripts/03_run_rx.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root_for_bpftrace  # bpftrace 需要 root 权限

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/RX_TRACEPOINT.log"

# 运行 bpftrace 脚本，运行时长为 BPFTRACE_DURATION（默认10秒）
run_bt "${LAB_DIR}/probes/rx.bt" "${OUT}" "${BPFTRACE_DURATION}"

echo "RX_TRACEPOINT=${OUT}"