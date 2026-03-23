#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

repo_url="https://github.com/pciutils/pciutils.git"
if [ -d "${PCIUTILS_SRC_DIR}/.git" ]; then
  echo "[day33] 已存在 pciutils 仓库：${PCIUTILS_SRC_DIR}"
else
  ensure_dir "$(dirname "${PCIUTILS_SRC_DIR}")"
  echo "[day33] 从 GitHub 获取 pciutils：${repo_url}"
  git clone "${repo_url}" "${PCIUTILS_SRC_DIR}"
fi
chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" 2>/dev/null || true
chmod +x "${PCIUTILS_SRC_DIR}/configure" 2>/dev/null || true
