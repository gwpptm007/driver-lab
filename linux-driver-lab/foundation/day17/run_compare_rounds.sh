#!/usr/bin/env bash
set -euo pipefail

# Day17 run_compare_rounds.sh
# ----------------------------
# 一次把 baseline / round1 / round2b 三个 profile 全跑完，并自动生成：
#   - compare-<timestamp>.csv
#   - compare-<timestamp>.md
#   - compare-<timestamp>-baseline_vs_round1.diff
#   - compare-<timestamp>-round1_vs_round2b.diff
#   - compare-<timestamp>-baseline_vs_round2b.diff
#
# 也就是说，跑完后不只知道“功能是否通过”，还知道：
#   1. 三轮最终 .config 是否真的变了
#   2. 内核 Image / rootfs.img sha256 是否真的变了
#   3. diff 文件里到底改了哪些 CONFIG_

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROFILES="${PROFILES:-baseline round1 round2b}"
PERF_REQUIRED="${PERF_REQUIRED:-yes}"
PERF_MODE="${PERF_MODE:-auto}"

info() {
    echo "[INFO] $*"
}

for profile in $PROFILES; do
    info "running profile: $profile"
    PERF_REQUIRED="$PERF_REQUIRED" PERF_MODE="$PERF_MODE" "$SCRIPT_DIR/run_profile_collect.sh" "$profile"
    echo
    echo "------------------------------------------------------------"
    echo

done

python3 "$SCRIPT_DIR/compare_results.py"
