#!/usr/bin/env bash
#===============================================================================
# 01_build_app.sh - 编译 media-gateway-lite
# 作用：使用 meson + ninja 编译 DPDK 应用
# 输出：二进制文件 MEDIA_BIN 和编译日志 BUILD.log
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 编译日志输出文件
OUT="${RECORD_DIR}/BUILD.log"

{
  echo "# BUILD_APP"        # 编译报告标题
  echo
  log_env                  # 输出环境变量（便于复现）
  echo

  # 进入 APP 目录执行编译
  cd "${APP_DIR}"
  echo "## make clean all"

  make clean              # 清理旧的构建产物
  make all                # 重新编译（meson + ninja）
  echo

  # 验证编译产物
  echo "## binary"
  ls -lh "${MEDIA_BIN}"    # 打印二进制文件大小
  file "${MEDIA_BIN}" || true   # 打印文件类型（确认静态链接）
} 2>&1 | tee "${OUT}"

echo "[OK] build saved: ${OUT}"