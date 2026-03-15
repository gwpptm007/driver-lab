#!/usr/bin/env bash
set -euo pipefail

# Day20 host entry
# ----------------
# 这个壳脚本做三件事：
# 1. 暴露更顺手的环境变量入口；
# 2. 调 Python 主程序执行真正回归；
# 3. 结束后顺手刷新一次 output/ 汇总视图。
#
# 常见用法：
#   ./run_day20_regression.sh
#   MODE=smoke ./run_day20_regression.sh
#   MODE=stress ./run_day20_regression.sh
#   MODE=all PERF_REQUIRED=yes TRACE_REQUIRED=yes ./run_day20_regression.sh
#   ./run_day20_regression.sh --dry-run

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
MODE="${MODE:-all}"
TRACE_REQUIRED="${TRACE_REQUIRED:-yes}"
PERF_REQUIRED="${PERF_REQUIRED:-yes}"
BOOT_TIMEOUT="${BOOT_TIMEOUT:-180}"
COMMAND_TIMEOUT="${COMMAND_TIMEOUT:-90}"
QEMU_MEMORY_MB="${QEMU_MEMORY_MB:-1024}"

set +e
python3 "$SCRIPT_DIR/run_day20_regression.py"     --mode "$MODE"     --trace-required "$TRACE_REQUIRED"     --perf-required "$PERF_REQUIRED"     --boot-timeout "$BOOT_TIMEOUT"     --command-timeout "$COMMAND_TIMEOUT"     --memory "$QEMU_MEMORY_MB"     "$@"
rc=$?
set -e

python3 "$SCRIPT_DIR/summarize_day20_records.py" >/dev/null 2>&1 || true
python3 "$SCRIPT_DIR/verify_day20_suite.py" >/dev/null 2>&1 || true
exit "$rc"
