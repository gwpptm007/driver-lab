#!/usr/bin/env bash
#============================================================
# 04_probe_native_copy.sh — 探测 native + copy 模式
#
# 功能：
#   运行 af_xdp_mode_probe，模式组合：native（驱动原生XDP）+ copy（拷贝模式）
#
# 目的：
#   验证实验机 网卡驱动 是否支持 native XDP attach（XDP_FLAGS_DRV_MODE）。
#   VMware vmxnet3 可能不支持，此脚本的失败本身也是有价值的记录。
#
# 使用：
#   sudo ./scripts/04_probe_native_copy.sh
#
# 输出：
#   - NATIVE_COPY_PROBE.log
#   - NATIVE_COPY_PROBE.rc（PROBE_RC 非 0 不一定是错误，需结合日志判断）
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "probe native/copy"

record_dir="$(latest_record_dir)"

run_probe "native" "copy" "${record_dir}/NATIVE_COPY_PROBE.log" "${record_dir}/NATIVE_COPY_PROBE.rc"