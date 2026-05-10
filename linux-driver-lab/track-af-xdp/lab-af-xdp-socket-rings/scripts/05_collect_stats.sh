#!/usr/bin/env bash
#============================================================
# 05_collect_stats.sh — 收集 AF_XDP 运行后的统计信息
#
# 功能：
#   在 smoke 测试完成后，收集实验机的网卡状态、XDP 状态、
#   内核 AF_XDP 相关日志，作为后续分析素材。
#
# 收集内容：
#   - ip link / ethtool -S（网卡收发统计）
#   - ip -d link（XDP attach 状态）
#   - 所有 AF_XDP 日志中的关键标记（UMEM_READY、FILL_RING_READY 等）
#
# 使用：
#   ./scripts/05_collect_stats.sh
#   或
#   ./scripts/05_collect_stats.sh /path/to/RECORD_DIR
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 支持指定记录目录（默认使用最新目录）
REC_DIR="${1:-$(latest_record_dir)}"
OUT="${REC_DIR}/COLLECT_STATS.txt"

{
    write_env_header
    echo

    # 网卡收发统计
    echo "== iface stats =="
    ip -s link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo

    # ethtool 详细统计（VMware vmxnet3 支持）
    echo "== ethtool stats head =="
    ethtool -S "${AF_XDP_IFACE}" 2>/dev/null | sed -n '1,120p' || true
    echo

    # XDP attach 状态
    echo "== xdp link state =="
    ip -d link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo

    # 从所有 AF_XDP 日志中提取关键标记
    echo "== latest AF_XDP logs =="
    for f in "${REC_DIR}"/AF_XDP_*.log; do
        [[ -f "${f}" ]] || continue
        echo "--- $(basename "${f}") ---"
        grep -E "UMEM_READY|XSK_SOCKET_READY|FILL_RING_READY|XDP_ATTACHED|XSKMAP_REGISTERED|AF_XDP_RINGS_READY|AF_XDP_FINAL_STATS|bye" "${f}" || true
    done
} 2>&1 | tee "${OUT}"