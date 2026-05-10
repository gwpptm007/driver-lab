#!/usr/bin/env bash
#============================================================
# 09_clean_runtime.sh — 清理 XDP attach 状态
#
# 功能：
#   从实验网卡上卸载所有类型的 XDP program（通用/驱动/硬件），
 *   恢复网卡到原始状态，避免影响后续实验。
 *
 * 使用：
 *   sudo ./scripts/09_clean_runtime.sh
 *
 * 清理顺序：
 *   xdp（通用）→ xdpgeneric（skb）→ xdpdrv（native）→ xdphw（offload）
 *   忽略"无 attach"的报错（ip link set xdp off 如果本来就没 attach 会报错，不影响）
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

    echo "Detach possible XDP program from ${AF_XDP_IFACE}"
    ip link set dev "${AF_XDP_IFACE}" xdp off 2>/dev/null || true
    ip link set dev "${AF_XDP_IFACE}" xdpgeneric off 2>/dev/null || true
    ip link set dev "${AF_XDP_IFACE}" xdpdrv off 2>/dev/null || true
    echo

    # 确认清理后的 XDP 状态（应该没有 XDP attach 了）
    ip -details link show "${AF_XDP_IFACE}" | grep -i xdp || true
} 2>&1 | tee "${out}"