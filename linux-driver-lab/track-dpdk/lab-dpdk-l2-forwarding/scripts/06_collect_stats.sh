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
    echo "## env"
    print_lab_env
    echo
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    echo
    echo "## interfaces"
    ip -br addr || true
    echo
    echo "## devbind"
    if devbind="$(find_devbind 2>/dev/null)"; then
        "${devbind}" --status || true
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
    echo
    echo "## l2fwd logs grep"
    grep -RInE 'EAL:|l2fwd-lite config|port [0-9]+ started|available/initialized ports|enter forwarding loop|software stats|rte_eth_stats|bye|failed|error|notice' "${RECORD_DIR}"/*.log 2>/dev/null || true
    echo
    echo "## dmesg dpdk/net/uio/vfio recent"
    dmesg 2>/dev/null | tail -n 200 | grep -Ei 'dpdk|uio|vfio|vmxnet|virtio|vhost|iommu|huge' || true
    if ! dmesg >/dev/null 2>&1; then
        echo "dmesg unavailable without sudo or kernel.dmesg_restrict=1"
    fi
    echo
    echo "## record files"
    find "${RECORD_DIR}" -maxdepth 1 -type f -printf '%f\n' | sort
} >> "${OUT}" 2>&1

echo "[OK] collect saved: ${OUT}"
