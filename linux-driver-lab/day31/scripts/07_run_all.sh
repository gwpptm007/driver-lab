#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day31] 开始执行 day31 全流程（RUN_ID=${RUN_ID}）"
bash "${DAY31_ROOT}/scripts/00_check_env.sh"
bash "${DAY31_ROOT}/scripts/03_build_tools.sh"
bash "${DAY31_ROOT}/scripts/09_build_day31_module.sh"
bash "${DAY31_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY31_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY31_ROOT}/scripts/06_run_qemu_day31.sh"
echo '[day31] QEMU 已退出，开始归档 records'
bash "${DAY31_ROOT}/scripts/08_extract_records.sh"
echo "[day31] day31 全流程执行结束，请查看 records/${RUN_ID}/"
