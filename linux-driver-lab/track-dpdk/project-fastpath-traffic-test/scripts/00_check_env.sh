#!/usr/bin/env bash
#===============================================================================
# 00_check_env.sh - 环境检查脚本
# 作用：检查 build 工具、DPDK binary、PCI 网卡状态
# 输出：records/<tag>/ENV_CHECK.txt
#===============================================================================
source "$(dirname "$0")/common.sh"

OUT="${RECORD_DIR}/ENV_CHECK.txt"
{
  echo "# ENV_CHECK"
  echo
  log_env
  echo
  echo "## system"
  uname -a || true
  lsb_release -a 2>/dev/null || true
  echo
  echo "## tools"
  for c in meson ninja pkg-config python3; do
    if command -v "$c" >/dev/null 2>&1; then
      echo "$c: $(command -v "$c")"
      "$c" --version 2>/dev/null | head -1 || true
    else
      echo "$c: NOT_FOUND"
    fi
  done
  echo
  echo "## fastpath binary"
  if [[ -x "${FASTPATH_BIN}" ]]; then
    ls -lh "${FASTPATH_BIN}"
    file "${FASTPATH_BIN}" || true
  else
    echo "FASTPATH_BIN_NOT_FOUND: ${FASTPATH_BIN}"
  fi
  echo
  echo "## dpdk-devbind"
  if command -v dpdk-devbind.py >/dev/null 2>&1; then
    dpdk-devbind.py --status || true
  else
    echo "dpdk-devbind.py: NOT_FOUND"
  fi
} | tee "${OUT}"

echo "[OK] env check saved: ${OUT}"
