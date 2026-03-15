#!/bin/sh
set -eu

# Day20 smoke：验证最短主线没坏。
# 检查顺序：
# 1. 基础挂载
# 2. insmod /demo_regmap.ko
# 3. snapshot 可读
# 4. trigger 可写
# 5. 前后 snapshot 留档
# 6. rmmod 正常
# 7. dmesg 无明显严重错误

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=/dev/null
. "$SCRIPT_DIR/guest_day20_common.sh"

DEMO_MODULE="${DEMO_MODULE:-/demo_regmap.ko}"
DEMO_DIR="${DEMO_DIR:-/sys/kernel/debug/demo_regmap}"
TRIGGER_COUNT="${TRIGGER_COUNT:-3}"

: > "$PASS_ENV"
: > "$SUMMARY_TXT"
: > "$DAY20_OUTDIR/smoke.log"

summary_append "Day20 smoke regression"
prepare_basic_fs
pick_tracing_dir
capture_meminfo

if [ -f "$DEMO_MODULE" ]; then
    kv_append DEMO_MODULE_PRESENT 1
else
    kv_append DEMO_MODULE_PRESENT 0
    summary_append "- FAIL: demo module missing: $DEMO_MODULE"
fi

if insmod "$DEMO_MODULE" >> "$DAY20_OUTDIR/smoke.log" 2>&1; then
    kv_append DEMO_INSMOD_OK 1
    summary_append "- PASS: insmod $DEMO_MODULE"
else
    kv_append DEMO_INSMOD_OK 0
    summary_append "- FAIL: insmod $DEMO_MODULE"
fi

sleep 1

if [ -f "$DEMO_DIR/snapshot" ] && cat "$DEMO_DIR/snapshot" > "$DAY20_OUTDIR/snapshot_before.txt" 2>> "$DAY20_OUTDIR/smoke.log"; then
    kv_append SNAPSHOT_OK 1
    summary_append "- PASS: snapshot readable"
else
    kv_append SNAPSHOT_OK 0
    summary_append "- FAIL: snapshot unreadable"
fi

if [ -f "$DEMO_DIR/trigger" ] && echo "$TRIGGER_COUNT" > "$DEMO_DIR/trigger" 2>> "$DAY20_OUTDIR/smoke.log"; then
    kv_append TRIGGER_OK 1
    summary_append "- PASS: trigger writable"
else
    kv_append TRIGGER_OK 0
    summary_append "- FAIL: trigger write failed"
fi

if [ -f "$DEMO_DIR/snapshot" ] && cat "$DEMO_DIR/snapshot" > "$DAY20_OUTDIR/snapshot_after.txt" 2>> "$DAY20_OUTDIR/smoke.log"; then
    kv_append SNAPSHOT_AFTER_OK 1
else
    kv_append SNAPSHOT_AFTER_OK 0
fi

if [ -s "$DAY20_OUTDIR/snapshot_before.txt" ] && [ -s "$DAY20_OUTDIR/snapshot_after.txt" ]; then
    kv_append SNAPSHOT_NONEMPTY 1
else
    kv_append SNAPSHOT_NONEMPTY 0
fi

if rmmod demo_regmap >> "$DAY20_OUTDIR/smoke.log" 2>&1; then
    kv_append RMMOD_OK 1
    summary_append "- PASS: rmmod demo_regmap"
else
    kv_append RMMOD_OK 0
    summary_append "- FAIL: rmmod demo_regmap"
fi

capture_modules
scan_dmesg_tail
emit_env_and_files
