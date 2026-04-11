#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day26] 开始执行 day26 全流程（RUN_ID=${RUN_ID}）"
bash "${DAY26_ROOT}/scripts/00_check_env.sh"
bash "${DAY26_ROOT}/scripts/03_build_tools.sh"
if [ ! -f "${DAY26_ROOT}/driver/day26_edu_tool.ko" ]; then
    echo "[day26][WARN] run 流程检测到 day26_edu_tool.ko 缺失，先自动构建模块。"
    bash "${DAY26_ROOT}/scripts/09_build_day26_module.sh"
fi
bash "${DAY26_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY26_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY26_ROOT}/scripts/06_run_qemu_day26.sh"
echo "[day26] QEMU 已退出，开始归档 records"
bash "${DAY26_ROOT}/scripts/08_extract_records.sh"
echo "[day26] day26 全流程执行结束，请查看 records/${RUN_ID}/"
