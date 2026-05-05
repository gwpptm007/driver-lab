#!/usr/bin/env bash
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
    echo "## date"
    date
    echo
    echo "## project env"
    print_project_env
    echo
    echo "## app binary"
    ls -lh "${APP_BIN}" 2>/dev/null || true
    echo
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    mount | grep hugetlbfs || true
    echo
    echo "## dpdk-devbind"
    if devbind="$(find_devbind 2>/dev/null)"; then
        "${devbind}" --status || true
    else
        echo "dpdk-devbind not found"
    fi
    echo
    echo "## ip link"
    ip -br link || true
    echo
    echo "## recent app evidence"
    grep -R "fastpath-lite config\|enter fastpath loop\|software stats\|rte_eth_stats\|rewrite rules\|bye" "${RECORD_DIR}"/*.log 2>/dev/null || true
    echo
    echo "## dmesg related"
    dmesg | tail -n 120 2>&1 || true
} >> "${OUT}" 2>&1

echo "[OK] stats saved: ${OUT}"
