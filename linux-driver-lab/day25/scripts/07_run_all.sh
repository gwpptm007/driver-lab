#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# day25 一键入口：检查环境 -> 组 rootfs -> 准备 runtime dir -> 跑 QEMU -> 抽取 records

echo "[day25] 开始执行 day25 全流程（RUN_ID=${RUN_ID}）"
"${DAY25_ROOT}/scripts/00_check_env.sh"
"${DAY25_ROOT}/scripts/04_prepare_rootfs.sh"
"${DAY25_ROOT}/scripts/05_prepare_runtime_dir.sh"
"${DAY25_ROOT}/scripts/06_run_qemu_day25.sh"
echo "[day25] QEMU 已退出，开始归档 records"
"${DAY25_ROOT}/scripts/08_extract_records.sh"
echo "[day25] day25 全流程执行结束，请查看 records/${RUN_ID}/"
