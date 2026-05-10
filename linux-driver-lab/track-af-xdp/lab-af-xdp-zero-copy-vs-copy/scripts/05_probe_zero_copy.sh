#!/usr/bin/env bash
#============================================================
# 05_probe_zero_copy.sh — 探测 native + zero-copy 模式
#
# 功能：
#   运行 af_xdp_mode_probe，模式组合：native（驱动原生XDP）+ zero-copy（零拷贝）
#
# 目的：
#   探测网卡和驱动是否支持 AF_XDP zero-copy。
 *   zero-copy 依赖：
 *     - 网卡驱动支持 native XDP（XDP_FLAGS_DRV_MODE）
 *     - 驱动实现 AF_XDP zero-copy（不是所有驱动都有）
 *     - 队列、UMEM 参数满足要求
 *
 * VMware vmxnet3 很可能不支持，失败本身也是有效记录。
 * 失败时检查日志中的 xsk_socket__create 错误信息。
 *
 * 使用：
 *   sudo ./scripts/05_probe_zero_copy.sh
#
 * 输出：
 *   - ZERO_COPY_PROBE.log
 *   - ZERO_COPY_PROBE.rc
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "probe zero-copy"

record_dir="$(latest_record_dir)"

# Native + zero-copy 是有意义的 ZC 探测，在不支持的 NIC 上会失败
run_probe "native" "zero-copy" "${record_dir}/ZERO_COPY_PROBE.log" "${record_dir}/ZERO_COPY_PROBE.rc"