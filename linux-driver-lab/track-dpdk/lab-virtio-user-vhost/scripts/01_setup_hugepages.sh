#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/HUGEPAGE_SETUP.txt"
: > "${OUT}"

{
    echo "# HUGEPAGE_SETUP"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Before"
    grep Huge /proc/meminfo || true
    mount | grep -E "hugetlbfs|${HUGEPAGE_MOUNT}" || true
    echo
    echo "## Apply"
    echo "HUGEPAGES=${HUGEPAGES}"
    echo "HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}"
} >> "${OUT}" 2>&1

mkdir -p "${HUGEPAGE_MOUNT}"
echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages
if ! mountpoint -q "${HUGEPAGE_MOUNT}"; then
    mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
fi

{
    echo
    echo "## After"
    grep Huge /proc/meminfo || true
    mount | grep -E "hugetlbfs|${HUGEPAGE_MOUNT}" || true
    df -h "${HUGEPAGE_MOUNT}" || true
} >> "${OUT}" 2>&1

cat <<EOF_OUT
[OK] Hugepage setup saved:
${OUT}

Next:
  sudo ./scripts/02_run_virtio_user_vhost_pair.sh
EOF_OUT
