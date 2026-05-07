#!/usr/bin/env bash
#===============================================================================
# 09_clean_runtime.sh - 清理运行时状态
# 作用：检查 media-gateway-lite 和 dpdk-testpmd 进程是否仍在运行
# 说明：
#   - 默认不自动杀进程（避免误杀），仅作展示
#   - 手动确认后可使用 pkill 或 kill 命令终止
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 清理状态日志
OUT="${RECORD_DIR}/CLEAN_RUNTIME.txt"

{
  echo "# CLEAN_RUNTIME"
  echo
  log_env                  # 输出环境变量（便于确认环境）
  echo

  echo "## remaining media/testpmd processes"
  # pgrep -a: 显示匹配进程的所有参数（完整命令行）
  # -f: 匹配命令行中的字符串（不只是进程名）
  pgrep -a -f 'media-gateway-lite|dpdk-testpmd|testpmd' || true
  echo

  # 提示用户手动处理
  echo "No process is killed automatically by default. Kill manually if needed."
} | tee "${OUT}"

echo "[OK] clean runtime saved: ${OUT}"