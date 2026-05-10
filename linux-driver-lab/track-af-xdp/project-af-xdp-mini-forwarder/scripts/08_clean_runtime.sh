#!/usr/bin/env bash
#============================================================
# 08_clean_runtime.sh — 清理 XDP attach 状态
#
# 功能：
#   从实验网卡上卸载所有类型的 XDP program（通用/驱动），
#   恢复网卡到原始状态，避免影响后续实验。
#
# 使用：
#   sudo ./scripts/08_clean_runtime.sh
#
# 清理顺序：
#   xdp（通用）→ xdpgeneric（skb）→ xdpdrv（native）
#   忽略"无 attach"的报错（不影响）
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "clean AF_XDP runtime"

record_dir="$(latest_record_dir)"
out="${record_dir}/CLEAN_RUNTIME.txt"

{
    write_env_header
    echo

    echo "== detach xdp best effort =="
    ip link set dev "${AF_XDP_IFACE}" xdp off 2>/dev/null || true
    ip link set dev "${AF_XDP_IFACE}" xdpgeneric off 2>/dev/null || true
    echo

    echo "== after =="
    ip -details link show "${AF_XDP_IFACE}" || true
} 2>&1 | tee "${out}"