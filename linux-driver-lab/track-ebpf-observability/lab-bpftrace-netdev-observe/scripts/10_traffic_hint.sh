#!/usr/bin/env bash
#============================================================
# 09_traffic_hint.sh — 流量提示：如何产生测试流量
#
# 功能：
#   显示如何在观测时产生流量，帮助验证观测点是否正常工作
#
# 推荐的完整测试流程：
#   1. 清理 XDP 程序（如果有）
#   2. 运行各 tracepoint 观测脚本
#   3. 收集统计和生成评审报告
#
# 流量选项：
#   - 管理网口通常有 SSH/ping/apt 等流量，适合快速验证
#   - 目标网卡的流量需要另一台 VM/主机或外部测试仪
#
# 使用：
#   ./scripts/09_traffic_hint.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

cat <<EOF
Traffic hints for ${LAB_NAME}

Current target iface: ${BPFTRACE_IFACE}

Preferred retest flow:
  sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh
  sudo BPFTRACE_DURATION=15 ./scripts/03_run_tracepoint_rx.sh
  sudo BPFTRACE_DURATION=15 ./scripts/04_run_tracepoint_tx.sh
  sudo BPFTRACE_DURATION=15 ./scripts/05_run_softirq_observe.sh

Generate traffic while the observe script is running:
  - SSH/ping/apt/curl traffic usually appears on management iface ${BPFTRACE_MANAGEMENT_IFACE}
  - For ${BPFTRACE_IFACE}, use another VM/host in the same L2 network

Quick management-port smoke:
  BPFTRACE_IFACE=${BPFTRACE_MANAGEMENT_IFACE} sudo ./scripts/03_run_tracepoint_rx.sh
  BPFTRACE_IFACE=${BPFTRACE_MANAGEMENT_IFACE} sudo ./scripts/04_run_tracepoint_tx.sh

Then rerun:
  ./scripts/08_collect_stats.sh
  ./scripts/09_make_review_bundle.sh
EOF