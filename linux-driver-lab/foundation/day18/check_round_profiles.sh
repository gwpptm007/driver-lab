#!/usr/bin/env bash
set -euo pipefail

# Day18 compatibility wrapper
# ---------------------------
# day17 中这个名字主要用来检查 round1/round2b。
# day18 已经换成 baseline / round2b_legacy / classified，
# 因此这里直接转到新的等价性检查脚本。

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
exec "$SCRIPT_DIR/check_profile_equivalence.sh" "$@"
