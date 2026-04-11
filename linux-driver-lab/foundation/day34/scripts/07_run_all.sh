#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day34] 开始执行 day34 全流程（RUN_ID=${RUN_ID}）"
bash "${DAY34_ROOT}/scripts/00_check_env.sh"
bash "${DAY34_ROOT}/scripts/03_build_tools.sh"
bash "${DAY34_ROOT}/scripts/09_build_day34_module.sh"
bash "${DAY34_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY34_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY34_ROOT}/scripts/06_run_qemu_day34.sh"
echo '[day34] QEMU 已退出，开始归档 records'
bash "${DAY34_ROOT}/scripts/08_extract_records.sh"
bash "${DAY34_ROOT}/scripts/11_generate_stability_summary.sh" || true
echo "[day34] day34 全流程执行结束，请查看 records/${RUN_ID}/"
