#!/usr/bin/env bash
set -euo pipefail

# Day18 run_compare_profiles.sh
# -----------------------------
# 一次把 baseline / round2b_legacy / classified 三个 profile 全跑完，并自动生成：
#   - compare-<timestamp>.csv
#   - compare-<timestamp>.md
#   - compare-<timestamp>-baseline_vs_round2b_legacy.diff
#   - compare-<timestamp>-baseline_vs_classified.diff
#   - compare-<timestamp>-round2b_legacy_vs_classified.diff

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROFILES="${PROFILES:-baseline round2b_legacy classified}"
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
