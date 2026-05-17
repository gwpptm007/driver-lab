#!/usr/bin/env bash
# 脚本: 05_clean_runtime.sh
# 功能: 清理运行时遗留的 vhost-user socket 和 testpmd 进程
# 用法: sudo ./scripts/05_clean_runtime.sh
#
# 注意：只删除 socket 文件本身，不停止 testpmd 进程
# 如果仍有 testpmd 进程运行，则保留 socket 不删除（防止误删其他进程的 socket）

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（删除 socket 文件需要）
require_root_for_write
# 检查 socket 路径是否在安全目录
safe_socket_path_check

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/CLEAN_RUNTIME.txt"
: > "${OUT}"

{
    echo "# CLEAN_RUNTIME"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Before"
    ls -l "${VHOST_SOCKET}" 2>/dev/null || true
    ps -ef | grep -E "dpdk-testpmd|testpmd|${TESTPMD_FILE_PREFIX}" | grep -v grep || true
} >> "${OUT}" 2>&1

# 只删除 socket 如果没有匹配的 testpmd 进程仍在运行
# 防止误删其他进程的 socket 文件
if ps -ef | grep -E "dpdk-testpmd|testpmd|${TESTPMD_FILE_PREFIX}" | grep -v grep >/dev/null 2>&1; then
    echo "WARN: testpmd-like process still exists; not removing socket." >> "${OUT}"
else
    rm -f "${VHOST_SOCKET}"
fi

{
    echo
    echo "## After"
    ls -l "${VHOST_SOCKET}" 2>/dev/null || true
    ps -ef | grep -E "dpdk-testpmd|testpmd|${TESTPMD_FILE_PREFIX}" | grep -v grep || true
} >> "${OUT}" 2>&1

cat <<EOF_OUT
[OK] Clean runtime saved:
${OUT}
EOF_OUT
