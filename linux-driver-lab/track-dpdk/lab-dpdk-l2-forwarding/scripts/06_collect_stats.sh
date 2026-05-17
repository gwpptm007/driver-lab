#!/usr/bin/env bash
# =============================================================================
# 06_collect_stats.sh — 收集运行后统计信息（只读，不修改系统）
#
# 在 l2fwd-lite 运行结束后执行，汇总：
#   1. 当前 hugepage 使用情况
#   2. 网卡接口状态
#   3. dpdk-devbind 绑定状态
#   4. 从运行日志中提取关键输出（EAL/port started/forwarding/stats/error）
#   5. dmesg 中 DPDK/UIO/VFIO 相关的内核日志
#   6. 记录目录中的文件清单
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/COLLECT_STATS.txt
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/COLLECT_STATS.txt"
: > "${OUT}"
append_command_log "${RECORD_DIR}" "$0"

{
    echo "# COLLECT_STATS"
    echo
    # ── 环境变量 ─────────────────────────────────────────────
    echo "## env"
    print_lab_env
    echo
    # ── Hugepage 使用情况 ─────────────────────────────────────
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    echo
    # ── 网络接口状态 ─────────────────────────────────────────
    echo "## interfaces"
    ip -br addr || true
    echo
    # ── dpdk-devbind 绑定状态 ────────────────────────────────
    echo "## devbind"
    if devbind="$(find_devbind 2>/dev/null)"; then
        "${devbind}" --status || true
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
    echo
    # ── 从运行日志中提取关键输出 ──────────────────────────────
    # grep 所有 .log 文件中的标志性输出，快速判断实验是否成功
    echo "## l2fwd logs grep"
    grep -RInE 'EAL:|l2fwd-lite config|port [0-9]+ started|available/initialized ports|enter forwarding loop|software stats|rte_eth_stats|bye|failed|error|notice' "${RECORD_DIR}"/*.log 2>/dev/null || true
    echo
    # ── 内核日志中 DPDK 相关信息 ──────────────────────────────
    # 可能需要 sudo 才能读 dmesg
    echo "## dmesg dpdk/net/uio/vfio recent"
    dmesg 2>/dev/null | tail -n 200 | grep -Ei 'dpdk|uio|vfio|vmxnet|virtio|vhost|iommu|huge' || true
    if ! dmesg >/dev/null 2>&1; then
        echo "dmesg unavailable without sudo or kernel.dmesg_restrict=1"
    fi
    echo
    # ── 记录目录中的文件清单 ─────────────────────────────────
    echo "## record files"
    find "${RECORD_DIR}" -maxdepth 1 -type f -printf '%f\n' | sort
} >> "${OUT}" 2>&1

echo "[OK] collect saved: ${OUT}"
