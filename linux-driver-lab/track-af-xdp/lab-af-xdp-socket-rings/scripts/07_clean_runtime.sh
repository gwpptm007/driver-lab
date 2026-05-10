#!/usr/bin/env bash
#============================================================
# 07_clean_runtime.sh — 清理 XDP attach 状态
#
# 功能：
#   从实验网卡上卸载所有类型的 XDP program（通用/驱动/硬件），
#   恢复网卡到原始状态，避免影响后续实验。
#
# 使用：
#   sudo ./scripts/07_clean_runtime.sh
#
# 注意：
#   清理顺序：xdpgeneric → xdpdrv → xdp（对应 skb / native / HW 三种模式）
#   忽略"无 attach"的报错（ip link set xdp off 如果本来就没 attach 会报错，不影响）
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/CLEAN_RUNTIME.txt"

{
    write_env_header
    echo

    # 清理前的 XDP 状态
    echo "== before =="
    ip -d link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo

    # 依次尝试卸载所有类型的 XDP program
    echo "Detaching XDP from ${AF_XDP_IFACE}; ignore errors if none is attached."
    ip link set dev "${AF_XDP_IFACE}" xdp off 2>/dev/null || true
    ip link set dev "${AF_XDP_IFACE}" xdpgeneric off 2>/dev/null || true
    ip link set dev "${AF_XDP_IFACE}" xdpdrv off 2>/dev/null || true
    echo

    # 清理后的 XDP 状态
    echo "== after =="
    ip -d link show "${AF_XDP_IFACE}" 2>/dev/null || true
    echo

    echo "CLEAN_RESULT=DONE"
} 2>&1 | tee "${OUT}"