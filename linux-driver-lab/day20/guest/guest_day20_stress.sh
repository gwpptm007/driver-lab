#!/bin/sh
set -eu

# Day20 stress：沿用 Day06 的回归思路，做最小压力检查。
# 目标不是做长时间 benchmark，而是快速回答：
# 1. 模块多次装卸是否稳定；
# 2. trigger 连续写入是否稳定；
# 3. dmesg 是否出现明显 Oops/BUG/Call trace。

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=/dev/null
. "$SCRIPT_DIR/guest_day20_common.sh"

DEMO_MODULE="${DEMO_MODULE:-/demo_regmap.ko}"
DEMO_DIR="${DEMO_DIR:-/sys/kernel/debug/demo_regmap}"
STRESS_LOOPS="${STRESS_LOOPS:-5}"
TRIGGERS_PER_LOOP="${TRIGGERS_PER_LOOP:-5}"

: > "$PASS_ENV"
: > "$SUMMARY_TXT"
: > "$DAY20_OUTDIR/stress.log"

summary_append "Day20 stress regression"
prepare_basic_fs
pick_tracing_dir
capture_meminfo

all_ok=1
loop_done=0

while [ "$loop_done" -lt "$STRESS_LOOPS" ]; do
    loop_done=$((loop_done + 1))
    echo "[stress] round=$loop_done insmod" >> "$DAY20_OUTDIR/stress.log"
    if ! insmod "$DEMO_MODULE" >> "$DAY20_OUTDIR/stress.log" 2>&1; then
        all_ok=0
        summary_append "- FAIL: insmod failed in round $loop_done"
        break
    fi

    i=0
    while [ "$i" -lt "$TRIGGERS_PER_LOOP" ]; do
        i=$((i + 1))
        if ! echo "$i" > "$DEMO_DIR/trigger" 2>> "$DAY20_OUTDIR/stress.log"; then
            all_ok=0
            summary_append "- FAIL: trigger write failed in round $loop_done step $i"
            break 2
        fi
        cat "$DEMO_DIR/snapshot" >> "$DAY20_OUTDIR/stress.log" 2>/dev/null || true
    done

    echo "[stress] round=$loop_done rmmod" >> "$DAY20_OUTDIR/stress.log"
    if ! rmmod demo_regmap >> "$DAY20_OUTDIR/stress.log" 2>&1; then
        all_ok=0
        summary_append "- FAIL: rmmod failed in round $loop_done"
        break
    fi

done

kv_append STRESS_LOOPS "$STRESS_LOOPS"
kv_append STRESS_ROUNDS_DONE "$loop_done"
if [ "$all_ok" -eq 1 ] && [ "$loop_done" -eq "$STRESS_LOOPS" ]; then
    kv_append STRESS_OK 1
    summary_append "- PASS: stress loops completed"
else
    kv_append STRESS_OK 0
fi

capture_modules
scan_dmesg_tail
emit_env_and_files
