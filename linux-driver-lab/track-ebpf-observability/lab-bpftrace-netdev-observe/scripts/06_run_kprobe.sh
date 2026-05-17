#!/usr/bin/env bash
#============================================================
# 06_run_kprobe.sh — 运行可选的 kprobe 探针观测
#
# 功能：
#   运行可选的 kprobe 探针（如 napi_poll、netif_receive_skb 等）
#   这些探针因 BTF 问题、内联函数、notrace 标记可能不可用
#
# 设计原则：
#   kprobe 失败只记录为 NOTE，不作为 lab 失败
#   tracepoint 是验收路径，kprobe 是可选探索
#
# 输出：
#   records/.../KPROBE_OPTIONAL.log
#
# 使用：
#   sudo ./scripts/06_run_kprobe.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root_for_bpftrace  # bpftrace 需要 root 权限

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/KPROBE_OPTIONAL.log"

# run_bt_optional 失败时不报错，结果标记为 OPTIONAL_RESULT
run_bt_optional "${LAB_DIR}/probes/kprobe.bt" "${OUT}" "${BPFTRACE_DURATION}"

echo "KPROBE_OPTIONAL=${OUT}"