#!/usr/bin/env bash
#============================================================
# 03_run_forwarder_drop_smoke.sh — 运行 drop 模式 smoke 测试
#
# 功能：
#   以 drop 模式运行转发器，验证：
#   - UMEM / socket / XDP attach / XSKMAP 注册 路径正常
#   - 程序能稳定运行指定时长
#
# drop 模式：收到包后直接丢弃，不发包。
# 用于验证 AF_XDP 基本路径，不依赖流量。
#
# 使用：
#   sudo ./scripts/03_run_forwarder_drop_smoke.sh
#
# 输出：
#   - FORWARDER_DROP.log
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "run AF_XDP forwarder drop smoke"

record_dir="$(latest_record_dir)"
out="${record_dir}/FORWARDER_DROP.log"

run_forwarder "drop" "${out}"