#!/usr/bin/env bash
#============================================================
# 04_run_tx.sh — 观测 TX（发送）路径的 tracepoint
#
# 功能：
#   运行 bpftrace 脚本观测网络卡发送路径：
#   协议栈 → net_dev_queue tracepoint → net_dev_xmit tracepoint → NIC TX
#
# 探针：
#   - tracepoint:net:net_dev_queue  — TX 排队
#   - tracepoint:net:net_dev_xmit   — TX 发送
#
# 统计：
#   - @tx_queue_total: 总入队包数
#   - @tx_xmit_total: 总发送包数
#   - @tx_queue_cpu[cpu]: 每 CPU 入队数
#   - @tx_xmit_cpu[cpu]: 每 CPU 发送数
#
# 依赖：
#   - BPFTRACE_IFACE 指定的网卡有真实流量发出
#   - 无流量时计数器为零（不代表脚本错误）
#
# 输出：
#   records/.../TX_TRACEPOINT.log
#
# 使用：
#   sudo ./scripts/04_run_tx.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root_for_bpftrace  # bpftrace 需要 root 权限

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/TX_TRACEPOINT.log"

# 运行 bpftrace 脚本，运行时长为 BPFTRACE_DURATION（默认10秒）
run_bt "${LAB_DIR}/probes/tx.bt" "${OUT}" "${BPFTRACE_DURATION}"

echo "TX_TRACEPOINT=${OUT}"