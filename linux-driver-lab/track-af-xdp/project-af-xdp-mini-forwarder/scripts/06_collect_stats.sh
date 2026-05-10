#!/usr/bin/env bash
#============================================================
# 06_collect_stats.sh — 收集转发器运行后的统计信息
#
# 功能：
#   在 smoke 测试完成后，收集最终状态：
#   网卡收发统计、XDP attach 状态、转发器统计解析。
#
# 使用：
#   ./scripts/06_collect_stats.sh
#   或
#   ./scripts/06_collect_stats.sh /path/to/RECORD_DIR
#
# 输出：
#   COLLECT_STATS.txt
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="${1:-$(latest_record_dir)}"
out="${record_dir}/COLLECT_STATS.txt"

{
    write_env_header
    echo

    echo "== iface stats =="
    ip -s link show "${AF_XDP_IFACE}" || true
    echo

    echo "== xdp state =="
    ip -details link show "${AF_XDP_IFACE}" || true
    echo

    echo "== parsed forwarder stats =="
    python3 "${PROJECT_DIR}/tools/parse_forwarder_stats.py" "${record_dir}"/*.log || true
} 2>&1 | tee "${out}"