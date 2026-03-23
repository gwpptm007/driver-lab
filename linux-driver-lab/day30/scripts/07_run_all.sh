#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day30] 开始执行 day30 全流程（RUN_ID=${RUN_ID}）"
# day30 的自动化顺序和 day29 类似，但这里多了一条 mmap 失败路径验证，
# 所以 records 不只要看 run_ok/verify_ok，也要看 invalid-mmap-* 两个样例。
bash "${DAY30_ROOT}/scripts/00_check_env.sh"
bash "${DAY30_ROOT}/scripts/03_build_tools.sh"
bash "${DAY30_ROOT}/scripts/09_build_day30_module.sh"
bash "${DAY30_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY30_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY30_ROOT}/scripts/06_run_qemu_day30.sh"
echo '[day30] QEMU 已退出，开始归档 records'
bash "${DAY30_ROOT}/scripts/08_extract_records.sh"
echo "[day30] day30 全流程执行结束，请查看 records/${RUN_ID}/"
