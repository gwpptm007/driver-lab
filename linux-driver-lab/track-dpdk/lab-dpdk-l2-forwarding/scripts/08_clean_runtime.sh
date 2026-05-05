#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

OUT="${LAB_ROOT}/records/CLEAN_RUNTIME_$(timestamp).txt"
mkdir -p "${LAB_ROOT}/records"
: > "${OUT}"

{
    echo "# CLEAN_RUNTIME"
    echo
    echo "## kill possible l2fwd-lite processes owned by current user"
    pkill -f "${APP_BIN}" 2>/dev/null || true
    pkill -f "l2fwd-lite" 2>/dev/null || true
    echo "done"
    echo
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
} >> "${OUT}" 2>&1

echo "[OK] clean runtime saved: ${OUT}"
