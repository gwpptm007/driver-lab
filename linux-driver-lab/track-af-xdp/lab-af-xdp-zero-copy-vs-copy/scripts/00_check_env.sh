#!/usr/bin/env bash
#============================================================
# 00_check_env.sh — 环境检查脚本
#
# 功能：
#   检查 af-xdp-zero-copy-vs-copy 实验所需的工具链、库、网卡状态，
#   将结果写入 records/<时间戳>/ENV_CHECK.txt。
#
# 使用：
#   ./scripts/00_check_env.sh
#
# 检查内容：
#   - 工具链：clang / cc / make / ip / ethtool / bpftool / pkg-config
#   - libbpf 版本和连接参数
#   - 实验网卡的驱动信息（ethtool -i）
#   - 网卡支持的 offload 特性（ethtool -k）
#   - 当前 XDP attach 状态
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(make_record_dir)"
out="${record_dir}/ENV_CHECK.txt"

{
    write_env_header
    echo

    # 工具链检查
    echo "== tools =="
    for t in clang cc make ip ethtool bpftool pkg-config; do
        if command -v "$t" >/dev/null 2>&1; then
            echo "$t: $(command -v "$t")"
        else
            echo "$t: NOT_FOUND"
        fi
    done
    echo

    # libbpf 版本和编译参数
    echo "== libbpf pkg-config =="
    pkg-config --modversion libbpf 2>/dev/null || true
    pkg-config --cflags --libs libbpf 2>/dev/null || true
    echo

    # 网卡基本信息
    echo "== iface =="
    ip -details link show "${AF_XDP_IFACE}" || true
    echo

    # 网卡驱动信息
    echo "== ethtool -i =="
    ethtool -i "${AF_XDP_IFACE}" || true
    echo

    # 网卡 offload 特性（特别是 xdp 相关）
    echo "== ethtool -k selected =="
    ethtool -k "${AF_XDP_IFACE}" 2>/dev/null | grep -E 'rx|tx|xdp|ntuple|busy|scatter|generic' || true
    echo

    # 当前 XDP 状态（如果有的话）
    echo "== xdp state =="
    ip -details link show "${AF_XDP_IFACE}" | grep -i xdp || true
} | tee "${out}"

echo "RECORD_DIR=${record_dir}"