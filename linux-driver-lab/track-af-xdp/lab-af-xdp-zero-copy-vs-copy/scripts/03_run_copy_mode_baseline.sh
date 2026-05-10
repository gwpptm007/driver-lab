#!/usr/bin/env bash
#============================================================
# 03_run_copy_mode_baseline.sh — 运行 skb + copy 基线测试
#
# 功能：
#   运行 af_xdp_mode_probe，模式组合：skb（通用XDP）+ copy（拷贝模式）
#   这是兼容性最强的组合，在大多数驱动和虚拟网卡上都能工作。
#
# 使用：
#   sudo ./scripts/03_run_copy_mode_baseline.sh
#
# 输出：
#   - COPY_BASELINE.log（程序完整输出）
#   - COPY_BASELINE.rc（程序返回值：0=成功，非0=失败）
#
# 通过标准：
#   PROBE_RC=0 且 XSK_SOCKET_READY / XSKMAP_REGISTERED 均出现
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "run AF_XDP copy baseline"

record_dir="$(latest_record_dir)"

# run_probe(mode, bind, log_file, rc_file)
run_probe "skb" "copy" "${record_dir}/COPY_BASELINE.log" "${record_dir}/COPY_BASELINE.rc"