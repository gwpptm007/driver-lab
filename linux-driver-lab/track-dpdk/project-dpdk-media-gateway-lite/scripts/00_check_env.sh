#!/usr/bin/env bash
#===============================================================================
# 00_check_env.sh - 环境检查脚本
# 作用：检查构建工具、DPDK库、dpdk-devbind工具链是否存在
# 输出：检查结果写入 RECORDS_DIR/ENV_CHECK.txt
#===============================================================================
set -euo pipefail

# 引入公共变量和函数（PROJECT_ROOT, MEDIA_BIN, log_env 等）
source "$(dirname "$0")/common.sh"

# 环境检查结果输出文件
OUT="${RECORD_DIR}/ENV_CHECK.txt"

{
  echo "# ENV_CHECK"      # 检查报告标题
  echo
  log_env                  # 输出当前环境变量（调试用）
  echo

  #----------------------------------------
  # 系统信息
  #----------------------------------------
  echo "## system"
  uname -a || true
  lsb_release -a 2>/dev/null || true
  echo

  #----------------------------------------
  # 构建工具链检查
  #----------------------------------------
  echo "## tools"
  for c in meson ninja pkg-config cc gcc python3; do
    if command -v "$c" >/dev/null 2>&1; then
      # 工具存在：打印路径和版本
      echo "$c: $(command -v "$c")"
      "$c" --version 2>/dev/null | head -1 || true
    else
      # 工具缺失：标记 NOT_FOUND
      echo "$c: NOT_FOUND"
    fi
  done
  echo

  #----------------------------------------
  # DPDK 库和工具检查
  #----------------------------------------
  echo "## dpdk"
  if pkg-config --exists libdpdk; then
    # libdpdk 存在：打印版本号
    echo "libdpdk: FOUND $(pkg-config --modversion libdpdk 2>/dev/null || true)"
  else
    echo "libdpdk: NOT_FOUND"
  fi

  # 查找 dpdk-devbind.py（网卡绑定工具）
  if DPDK_DEVBIND=$(find_dpdk_devbind); then
    echo "dpdk-devbind: ${DPDK_DEVBIND}"
    "${DPDK_DEVBIND}" --status || true    # 打印当前网卡绑定状态
  else
    echo "dpdk-devbind: NOT_FOUND"
  fi
  echo

  #----------------------------------------
  # media-gateway-lite 二进制文件检查
  #----------------------------------------
  echo "## binary"
  if [[ -x "${MEDIA_BIN}" ]]; then
    ls -lh "${MEDIA_BIN}"                    # 打印文件大小和权限
    file "${MEDIA_BIN}" || true              # 打印文件类型（静态/动态链接）
  else
    echo "MEDIA_BIN_MISSING=${MEDIA_BIN}"
  fi
} | tee "${OUT}"

echo "[OK] env check saved: ${OUT}"