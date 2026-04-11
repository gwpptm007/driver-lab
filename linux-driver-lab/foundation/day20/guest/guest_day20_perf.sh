#!/bin/sh
set -eu

# Day20 perf：验证 perf 可执行、可列事件、可做最小 stat 冒烟。

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=/dev/null
. "$SCRIPT_DIR/guest_day20_common.sh"

PERF_REQUIRED="${PERF_REQUIRED:-yes}"

prepare_basic_fs
pick_tracing_dir
capture_meminfo

if ! command -v perf >/dev/null 2>&1; then
    kv_append PERF_OK 0
    summary_append "- FAIL: perf command missing"
    capture_modules
    scan_dmesg_tail
    emit_env_and_files
    [ "$PERF_REQUIRED" = yes ] && exit 1 || exit 0
fi

if perf --version > "$DAY20_OUTDIR/perf_version.txt" 2>&1; then
    kv_append PERF_BIN_OK 1
else
    kv_append PERF_BIN_OK 0
    kv_append PERF_OK 0
    summary_append "- FAIL: perf --version failed"
    capture_modules
    scan_dmesg_tail
    emit_env_and_files
    [ "$PERF_REQUIRED" = yes ] && exit 1 || exit 0
fi

perf list software | head -n 80 > "$DAY20_OUTDIR/perf_list.txt" 2>&1 || true

if perf stat -e task-clock -- /bin/true > "$DAY20_OUTDIR/perf_stat.txt" 2>&1; then
    kv_append PERF_OK 1
    summary_append "- PASS: perf stat basic smoke"
else
    kv_append PERF_OK 0
    summary_append "- FAIL: perf stat basic smoke"
fi

capture_modules
scan_dmesg_tail
emit_env_and_files
