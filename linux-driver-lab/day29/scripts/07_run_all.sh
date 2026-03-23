#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day29] 开始执行 day29 全流程（RUN_ID=${RUN_ID}）"
# run_all 的职责是把“检查 -> 构建 -> rootfs -> QEMU -> records”串起来，
# 让 Day29 能像一个学习实验包一样一条命令跑完整个闭环。
bash "${DAY29_ROOT}/scripts/00_check_env.sh"
bash "${DAY29_ROOT}/scripts/03_build_tools.sh"
# 这里始终重编一次模块，而不是只在 .ko 缺失时才构建。
# 原因：Day29 反复调试时，旧 .ko 残留很容易让 rootfs 打进过期模块，
# 导致“源码已更新，但 guest 里实际加载的还是旧模块”的假象。
bash "${DAY29_ROOT}/scripts/09_build_day29_module.sh"
bash "${DAY29_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY29_ROOT}/scripts/05_prepare_runtime_dir.sh"
bash "${DAY29_ROOT}/scripts/06_run_qemu_day29.sh"
echo '[day29] QEMU 已退出，开始归档 records'
bash "${DAY29_ROOT}/scripts/08_extract_records.sh"
echo "[day29] day29 全流程执行结束，请查看 records/${RUN_ID}/"
