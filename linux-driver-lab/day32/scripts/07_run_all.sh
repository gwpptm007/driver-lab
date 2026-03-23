#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day32] 开始执行 day32 全流程（RUN_ID=${RUN_ID}）"
bash "${DAY32_ROOT}/scripts/00_check_env.sh"
bash "${DAY32_ROOT}/scripts/03_build_tools.sh"
bash "${DAY32_ROOT}/scripts/09_build_day32_module.sh"
bash "${DAY32_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY32_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY32_ROOT}/scripts/06_run_qemu_day32.sh"
echo '[day32] QEMU 已退出，开始归档 records'
bash "${DAY32_ROOT}/scripts/08_extract_records.sh"
echo "[day32] day32 全流程执行结束，请查看 records/${RUN_ID}/"
