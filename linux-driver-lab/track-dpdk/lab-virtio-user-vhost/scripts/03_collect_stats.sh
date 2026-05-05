#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/POST_CHECK.txt"
: > "${OUT}"

{
    echo "# POST_CHECK"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Lab defaults"
    print_lab_env
    echo
} >> "${OUT}"

run_capture "${OUT}" bash -c 'grep Huge /proc/meminfo || true'
run_capture "${OUT}" bash -c 'mount | grep -E "hugetlbfs|/mnt/huge" || true'
run_capture "${OUT}" bash -c "ls -l '${VHOST_SOCKET}' 2>/dev/null || true"
run_capture "${OUT}" bash -c 'ss -xl 2>/dev/null | grep -E "vhost|dpdk|sock" || true'
run_capture "${OUT}" bash -c 'ps -ef | grep -E "dpdk-testpmd|testpmd|vhost_backend|virtio_frontend" | grep -v grep || true'
run_capture "${OUT}" bash -c 'dmesg | grep -Ei "vhost|virtio|huge|vfio|uio|dpdk" | tail -n 120 || true'

cat <<EOF_OUT
[OK] Post check saved:
${OUT}

Next:
  ./scripts/04_make_review_bundle.sh
EOF_OUT
