#!/usr/bin/env bash
set -euo pipefail

# Day18 compatibility wrapper
# ---------------------------
# 为了兼容 day17 的命名习惯，保留 run_compare_rounds.sh，
# 但 day18 的正式入口已经改为 run_compare_profiles.sh。

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
exec "$SCRIPT_DIR/run_compare_profiles.sh" "$@"
