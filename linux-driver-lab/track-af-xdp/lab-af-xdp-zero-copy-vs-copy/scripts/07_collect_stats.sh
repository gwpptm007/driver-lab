#!/usr/bin/env bash
#============================================================
# 07_collect_stats.sh — 收集各模式探测后的统计信息
#
# 功能：
#   在所有 probe 完成后，收集实验机的最终状态信息：
#   网卡收发统计、XDP attach 状态、BPF program 列表、驱动统计。
#
# 使用：
#   ./scripts/07_collect_stats.sh
#   或
#   ./scripts/07_collect_stats.sh /path/to/RECORD_DIR
#
# 输出：
#   COLLECT_STATS.txt
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="${1:-$(latest_record_dir)}"
out="${record_dir}/COLLECT_STATS.txt"

{
    write_env_header
    echo

    # 网卡收发统计（-s = 详细）
    echo "== iface =="
    ip -s link show "${AF_XDP_IFACE}" || true
    echo

    # XDP attach 状态
    echo "== xdp state =="
    ip -details link show "${AF_XDP_IFACE}" | grep -i xdp || true
    echo

    # 当前加载的 BPF program 列表
    echo "== bpftool prog brief =="
    bpftool prog show 2>/dev/null | tail -40 || true
    echo

    # 驱动层收发统计（过滤 rx/tx/xdp/drop/err/queue/timeout 相关）
    echo "== ethtool -S selected =="
    ethtool -S "${AF_XDP_IFACE}" 2>/dev/null | grep -Ei 'rx|tx|xdp|drop|err|queue|timeout' | head -120 || true
} | tee "${out}"