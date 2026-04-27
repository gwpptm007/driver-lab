#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/ENV_CHECK.txt"
: > "${OUT}"

{
    echo "# ENV_CHECK"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Lab defaults"
    print_lab_env
    echo
} >> "${OUT}"

run_capture "${OUT}" uname -a
run_capture "${OUT}" bash -c 'lsb_release -a 2>/dev/null || cat /etc/os-release'
run_capture "${OUT}" bash -c 'id'
run_capture "${OUT}" bash -c 'nproc; lscpu | sed -n "1,25p"'
run_capture "${OUT}" ip -br addr
run_capture "${OUT}" ip -br link
run_capture "${OUT}" bash -c 'grep Huge /proc/meminfo || true'
run_capture "${OUT}" bash -c 'mount | grep -E "hugetlbfs|/mnt/huge" || true'
run_capture "${OUT}" bash -c 'ls -ld /dev/hugepages /mnt/huge 2>/dev/null || true'
run_capture "${OUT}" bash -c 'lsmod | grep -E "vfio|uio|vhost|tun|tap|vmxnet3" || true'
run_capture "${OUT}" bash -c 'ss -xl 2>/dev/null | grep -E "vhost|dpdk|sock" || true'
run_capture "${OUT}" bash -c 'dmesg | grep -Ei "vhost|huge|iommu|vfio|uio" | tail -n 80 || true'

{
    echo
    echo "## DPDK tool discovery"
    if testpmd="$(find_testpmd)"; then
        echo "testpmd: ${testpmd}"
        "${testpmd}" --version 2>/dev/null || true
    else
        echo "testpmd: NOT FOUND"
    fi
    echo
    echo "## vhost socket"
    if [[ -e "${VHOST_SOCKET}" ]]; then
        ls -l "${VHOST_SOCKET}" || true
    else
        echo "${VHOST_SOCKET}: not exists"
    fi
} >> "${OUT}" 2>&1

cat <<EOF_OUT

[OK] Environment check saved:
${OUT}

Next:
  sudo ./scripts/01_setup_hugepages.sh
EOF_OUT
