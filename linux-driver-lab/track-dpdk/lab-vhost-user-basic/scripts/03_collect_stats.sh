#!/usr/bin/env bash
# 脚本: 03_collect_stats.sh
# 功能: 收集 testpmd 运行后的系统状态、socket 信息、进程状态、内核日志
# 用法: ./scripts/03_collect_stats.sh

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

# 收集大页内存状态
run_capture "${OUT}" bash -c 'grep Huge /proc/meminfo || true'
# 收集 hugetlbfs 挂载状态
run_capture "${OUT}" bash -c 'mount | grep -E "hugetlbfs|/mnt/huge" || true'
# 检查 vhost socket 是否仍存在
run_capture "${OUT}" bash -c "ls -l '${VHOST_SOCKET}' 2>/dev/null || true"
# 检查 socket 是否在监听状态
run_capture "${OUT}" bash -c 'ss -xl 2>/dev/null | grep -E "vhost|dpdk|sock" || true'
# 检查是否有残留的 testpmd 进程
run_capture "${OUT}" bash -c 'ps -ef | grep -E "dpdk-testpmd|testpmd|vhost_basic" | grep -v grep || true'
# 收集 dmesg 中与 vhost/huge 相关的内核日志
run_capture "${OUT}" bash -c 'dmesg | grep -Ei "vhost|huge|vfio|uio|dpdk" | tail -n 100 || true'

cat <<EOF_OUT
[OK] Post check saved:
${OUT}

Next:
  ./scripts/04_make_review_bundle.sh
EOF_OUT
