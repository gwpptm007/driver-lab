#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ -d "${PCIUTILS_SRC_DIR}/.git" ]; then
  echo "[day34] pciutils 已存在：${PCIUTILS_SRC_DIR}"
else
  ensure_dir "$(dirname "${PCIUTILS_SRC_DIR}")"
  echo "[day34] 从 GitHub 获取 pciutils -> ${PCIUTILS_SRC_DIR}"
  git clone https://github.com/pciutils/pciutils.git "${PCIUTILS_SRC_DIR}"
fi
chmod +x "${PCIUTILS_SRC_DIR}/configure" 2>/dev/null || true
chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" 2>/dev/null || true
