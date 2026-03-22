#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 顶层一键流程：检查 -> 构建工具/模块 -> rootfs -> 启动 QEMU -> 提取 records。
# 这个脚本本身不做复杂逻辑，主要负责把各个子步骤串起来。

echo "[day27] 开始执行 day27 全流程（RUN_ID=${RUN_ID}）"
bash "${DAY27_ROOT}/scripts/00_check_env.sh"
bash "${DAY27_ROOT}/scripts/03_build_tools.sh"
if [ ! -f "${DAY27_ROOT}/driver/day27_edu_loop.ko" ]; then
  echo '[day27][WARN] run 流程检测到 day27_edu_loop.ko 缺失，先自动构建模块。'
  bash "${DAY27_ROOT}/scripts/09_build_day27_module.sh"
fi
bash "${DAY27_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY27_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY27_ROOT}/scripts/06_run_qemu_day27.sh"
echo '[day27] QEMU 已退出，开始归档 records'
bash "${DAY27_ROOT}/scripts/08_extract_records.sh"
echo "[day27] day27 全流程执行结束，请查看 records/${RUN_ID}/"
