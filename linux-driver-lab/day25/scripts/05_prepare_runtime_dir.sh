#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 每次运行都在 workdir/runs/<RUN_ID>/ 下落一份独立记录，
# 便于后续比较多轮实验结果。
rd="$(run_dir)"
ensure_dir "$rd"
echo "[day25] day25 运行目录已准备：$rd"
