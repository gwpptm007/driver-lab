#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 这个脚本只负责把 pciutils 源码拉到 day31/third_party/pciutils，
# 真正的 guest lspci 静态编译由 02_build_guest_lspci.sh 负责。
repo_url="${PCIUTILS_GIT_URL:-https://github.com/pciutils/pciutils.git}"
repo_dir="${PCIUTILS_SRC_DIR}"

ensure_dir "$(dirname "$repo_dir")"
if [ -d "$repo_dir/.git" ]; then
  echo "[day31] 已存在 pciutils git 仓库：$repo_dir"
  git -C "$repo_dir" remote -v || true
  exit 0
fi

if [ -e "$repo_dir" ] && [ ! -d "$repo_dir/.git" ]; then
  echo "[day31][ERROR] 目标路径已存在但不是 git 仓库：$repo_dir" >&2
  exit 1
fi

require_exec git git
echo "[day31] 从 GitHub 获取 pciutils：$repo_url"
git clone "$repo_url" "$repo_dir"
chmod +x "$repo_dir/lib/configure" 2>/dev/null || true
chmod +x "$repo_dir/configure" 2>/dev/null || true
echo "[day31] pciutils 获取完成：$repo_dir"
