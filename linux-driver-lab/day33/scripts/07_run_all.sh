#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day33] 开始执行 day33 全流程（RUN_ID=${RUN_ID}）"
bash "${DAY33_ROOT}/scripts/00_check_env.sh"
bash "${DAY33_ROOT}/scripts/03_build_tools.sh"
bash "${DAY33_ROOT}/scripts/09_build_day33_module.sh"
bash "${DAY33_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY33_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY33_ROOT}/scripts/06_run_qemu_day33.sh"
echo '[day33] QEMU 已退出，开始归档 records'
bash "${DAY33_ROOT}/scripts/08_extract_records.sh"
bash "${DAY33_ROOT}/scripts/11_generate_trace_summary.sh" || true
echo "[day33] day33 全流程执行结束，请查看 records/${RUN_ID}/"
