#!/usr/bin/env bash
#============================================================
# 04_run_forwarder_reflect_smoke.sh — 运行 reflect 模式 smoke 测试
#
# 功能：
#   以 reflect 模式运行转发器，验证：
#   - TX ring 写入 + sendto() 触发发送 路径正常
#   - COMPLETION ring 消费 + frame 归还 路径正常
#
# reflect 模式：收到包后从 TX ring 发回（loopback 自测）。
# 需要发送侧正常，不依赖外部流量，但依赖 TX 路径完整。
#
# 使用：
#   sudo ./scripts/04_run_forwarder_reflect_smoke.sh
#
# 输出：
#   - FORWARDER_REFLECT.log
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root
refuse_management_iface "run AF_XDP forwarder reflect smoke"

record_dir="$(latest_record_dir)"
out="${record_dir}/FORWARDER_REFLECT.log"

run_forwarder "reflect" "${out}"