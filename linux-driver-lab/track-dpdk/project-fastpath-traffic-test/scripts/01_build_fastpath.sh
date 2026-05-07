#!/usr/bin/env bash
#===============================================================================
# 01_build_fastpath.sh - 构建 fastpath-lite
# 作用：调用 project-user-space-fastpath 的 meson 构建脚本
# 输出：records/<tag>/BUILD.log + fastpath-lite binary
#===============================================================================
source "$(dirname "$0")/common.sh"

OUT="${RECORD_DIR}/BUILD.log"
{
  echo "# BUILD_FASTPATH"
  echo
  log_env
  echo
  echo "## call upstream build script"
  cd "${FASTPATH_PROJECT_DIR}"
  ./scripts/01_build_app.sh
  echo
  echo "## binary"
  ls -lh "${FASTPATH_BIN}"
  file "${FASTPATH_BIN}" || true
} | tee "${OUT}"

echo "[OK] build saved: ${OUT}"
