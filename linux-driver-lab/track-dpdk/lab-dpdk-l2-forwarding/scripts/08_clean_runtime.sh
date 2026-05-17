#!/usr/bin/env bash
# =============================================================================
# 08_clean_runtime.sh — 清理运行时产物（不修改系统配置）
#
# 轻量清理，只做两件事：
#   1. 杀死残留的 l2fwd-lite 进程
#   2. 记录当前 hugepage 状态
#
# 不会做以下操作（需要手动处理）：
#   - 不会卸载 uio_pci_generic 驱动
#   - 不会将网卡从 DPDK 驱动恢复到内核驱动
#   - 不会删除 records 目录中的实验记录
#
# 产出：records/CLEAN_RUNTIME_<timestamp>.txt
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

OUT="${LAB_ROOT}/records/CLEAN_RUNTIME_$(timestamp).txt"
mkdir -p "${LAB_ROOT}/records"
: > "${OUT}"

{
    echo "# CLEAN_RUNTIME"
    echo
    # ── 杀死残留进程 ─────────────────────────────────────────
    # l2fwd-lite 可能因为 Ctrl-C 不彻底等原因残留
    echo "## kill possible l2fwd-lite processes owned by current user"
    pkill -f "${APP_BIN}" 2>/dev/null || true
    pkill -f "l2fwd-lite" 2>/dev/null || true
    echo "done"
    echo
    # ── 记录 hugepage 状态 ───────────────────────────────────
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
} >> "${OUT}" 2>&1

echo "[OK] clean runtime saved: ${OUT}"
