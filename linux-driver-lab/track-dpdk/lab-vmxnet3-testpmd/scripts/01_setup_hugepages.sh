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
    echo "HUGEPAGES=${HUGEPAGES}"
    echo "HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}"
    echo
    echo "## Before"
    grep Huge /proc/meminfo || true
    mount | grep hugetlbfs || true
    echo
} >> "${OUT}"

mkdir -p "${HUGEPAGE_MOUNT}"

if ! mount | grep -q " ${HUGEPAGE_MOUNT} "; then
    mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
fi

echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages

{
    echo
    echo "## After"
    grep Huge /proc/meminfo || true
    mount | grep hugetlbfs || true
} >> "${OUT}" 2>&1

cat <<EOF

[OK] Hugepage setup saved:
${OUT}

Next:
  ./scripts/02_bind_vmxnet3.sh status
EOF
