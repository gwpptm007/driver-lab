#!/usr/bin/env bash
#============================================================
# 00_check_env.sh — 环境检查脚本
#
# 功能：
#   检查 project-af-xdp-mini-forwarder 实验所需的工具链、库、网卡状态，
#   将结果写入 records/<时间戳>/ENV_CHECK.txt。
#
# 使用：
#   ./scripts/00_check_env.sh
#
# 检查内容：
#   - 工具链：clang / cc / make / bpftool / ip / ethtool
#   - libbpf 版本
#   - 网卡基本信息、驱动、队列数
#   - 当前 XDP attach 状态
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(make_record_dir)"
out="${record_dir}/ENV_CHECK.txt"

{
    write_env_header
    echo

    echo "== tools =="
    for t in clang cc make bpftool ip ethtool; do
        printf '%-16s' "$t"
        command -v "$t" || true
    done
    echo

    echo "== libbpf pkg-config =="
    pkg-config --modversion libbpf 2>/dev/null || echo "libbpf pkg-config not found"
    echo

    echo "== iface =="
    ip link show "${AF_XDP_IFACE}" || true
    ethtool -i "${AF_XDP_IFACE}" || true
    echo

    echo "== xdp state =="
    ip -details link show "${AF_XDP_IFACE}" || true
} 2>&1 | tee "${out}"
echo "RECORD_DIR=${record_dir}"