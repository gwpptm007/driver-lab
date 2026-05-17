#!/usr/bin/env bash
#============================================================
# 01_list_probe_points.sh — 列出可用的 bpftrace 观测点
#
# 功能：
#   1. 使用 bpftrace -l 列出当前内核支持的探针类型
#   2. 检查必需的 tracepoint 是否存在（主路径）
#   3. 检查可选的 kprobe 是否存在（因内核/包差异可能不可用）
#
# 探针分类：
#   必需（tracepoint）：
#     - tracepoint:net:netif_receive_skb  — RX 路径
#     - tracepoint:net:net_dev_queue      — TX 排队
#     - tracepoint:net:net_dev_xmit       — TX 发送
#     - tracepoint:irq:softirq_entry/exit — softirq 统计
#   可选（kprobe，可能因 BTF/notrace 问题不可用）：
#     - kprobe:napi_poll          — NAPI 轮询
#     - kprobe:netif_receive_skb   — RX 路径
#     - kprobe:dev_queue_xmit      — TX 路径
#
# 输出：
#   records/YYYYMMDD-HHMMSS-bpftrace-netdev-observe/PROBE_POINTS.txt
#
# 使用：
#   ./scripts/01_list_probe_points.sh
#   ./scripts/01_list_probe_points.sh /path/to/optional/dir
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 获取记录目录（可选参数指定，或使用上一次的目录）
OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/PROBE_POINTS.txt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Is)"
    echo "KERNEL=$(uname -r)"
    echo
    echo "== required tracepoints =="
    # 检查必需的 tracepoint 是否存在（这是验收路径，稳定）
    for p in \
        'tracepoint:net:netif_receive_skb' \
        'tracepoint:net:net_dev_queue' \
        'tracepoint:net:net_dev_xmit' \
        'tracepoint:irq:softirq_entry' \
        'tracepoint:irq:softirq_exit'; do
        echo
        echo "--- ${p} ---"
        # sudo 必要，因为 bpftrace -l 需要访问内核信息
        sudo bpftrace -l "${p}" 2>&1 || true
    done
    echo
    echo "== optional kprobes =="
    # kprobe 可能因 BTF 问题、内联、notrace 标记而不可用
    # 失败只记录为 NOTE，不作为 lab 失败
    for p in 'kprobe:napi_poll' 'kprobe:netif_receive_skb' 'kprobe:dev_queue_xmit'; do
        echo
        echo "--- ${p} ---"
        sudo bpftrace -l "${p}" 2>&1 | head -40 || true
    done
    echo
    echo "== note =="
    echo "tracepoint probes are the acceptance path; kprobe probes are optional because BTF/notrace availability differs by kernel/package."
} | tee "${OUT}"

echo "PROBE_POINTS=${OUT}"