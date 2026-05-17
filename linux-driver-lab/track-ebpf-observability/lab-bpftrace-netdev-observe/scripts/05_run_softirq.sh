#!/usr/bin/env bash
#============================================================
# 05_run_softirq.sh — 观测网络 softirq（软中断）处理
#
# 功能：
#   运行 bpftrace 脚本观测网络相关的 softirq（软中断）统计：
#   - NET_RX: 收包软中断，处理网络接收
#   - NET_TX: 发包软中断，处理网络发送
#
# 探针：
#   tracepoint:irq:softirq_entry  — softirq 进入
#   tracepoint:irq:softirq_exit   — softirq 退出
#
# 统计：
#   - @softirq_entry[vec, cpu]: 每种软中断类型+CPU的进入次数
#   - @softirq_exit[vec, cpu]: 每种软中断类型+CPU的退出次数
#
# 用途：
#   确认软中断是否被触发，验证网络流量是否到达内核
#
# 输出：
#   records/.../SOFTIRQ_TRACEPOINT.log
#
# 使用：
#   sudo ./scripts/05_run_softirq.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root_for_bpftrace  # bpftrace 需要 root 权限

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/SOFTIRQ_TRACEPOINT.log"

run_bt "${LAB_DIR}/probes/softirq.bt" "${OUT}" "${BPFTRACE_DURATION}"

echo "SOFTIRQ_TRACEPOINT=${OUT}"