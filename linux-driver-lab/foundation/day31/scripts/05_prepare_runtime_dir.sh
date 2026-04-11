#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
ensure_dir "$rd"
echo "[day31] 运行目录已准备好：$rd"
