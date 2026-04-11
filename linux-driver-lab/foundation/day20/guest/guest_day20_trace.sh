#!/bin/sh
set -eu

# Day20 trace：检查 function_graph 仍保留，且能围绕 demo_regmap 生成最小 trace 文本。

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=/dev/null
. "$SCRIPT_DIR/guest_day20_common.sh"

DEMO_MODULE="${DEMO_MODULE:-/demo_regmap.ko}"
DEMO_DIR="${DEMO_DIR:-/sys/kernel/debug/demo_regmap}"
FTRACE_FILTER="${FTRACE_FILTER:-demo_regmap_trigger_write demo_regmap_irq_handler demo_regmap_workfn}"
TRACE_REQUIRED="${TRACE_REQUIRED:-yes}"

prepare_basic_fs
pick_tracing_dir
capture_meminfo

if [ -z "$TRACING_DIR" ] || [ ! -f "$TRACING_DIR/available_tracers" ]; then
    kv_append FGRAPH_OK 0
    summary_append "- FAIL: tracing dir missing"
    capture_modules
    scan_dmesg_tail
    emit_env_and_files
    exit 0
fi

if grep -wq function_graph "$TRACING_DIR/available_tracers"; then
    kv_append FUNCTION_GRAPH_PRESENT 1
else
    kv_append FUNCTION_GRAPH_PRESENT 0
    kv_append FGRAPH_OK 0
    summary_append "- FAIL: function_graph not found in available_tracers"
    capture_modules
    scan_dmesg_tail
    emit_env_and_files
    [ "$TRACE_REQUIRED" = yes ] && exit 1 || exit 0
fi

insmod "$DEMO_MODULE" > /dev/null 2>&1 || true
sleep 1

{
    echo 0 > "$TRACING_DIR/tracing_on"
    echo nop > "$TRACING_DIR/current_tracer"
    : > "$TRACING_DIR/set_ftrace_filter"
    for fn in $FTRACE_FILTER; do
        echo "$fn" >> "$TRACING_DIR/set_ftrace_filter" 2>/dev/null || true
    done
    : > "$TRACING_DIR/trace"
    echo function_graph > "$TRACING_DIR/current_tracer"
    echo 1 > "$TRACING_DIR/tracing_on"
    echo 1 > "$DEMO_DIR/trigger"
    sleep 1
    echo 0 > "$TRACING_DIR/tracing_on"
    cat "$TRACING_DIR/trace" > "$DAY20_OUTDIR/trace.log"
    grep -E 'demo_regmap_trigger_write|demo_regmap_irq_handler|demo_regmap_workfn' "$DAY20_OUTDIR/trace.log" > "$DAY20_OUTDIR/trace_excerpt.txt" 2>/dev/null || true
    echo nop > "$TRACING_DIR/current_tracer"
} 2>> "$DAY20_OUTDIR/trace.log" || true

if [ -s "$DAY20_OUTDIR/trace_excerpt.txt" ]; then
    kv_append FGRAPH_OK 1
    summary_append "- PASS: function_graph trace excerpt captured"
else
    kv_append FGRAPH_OK 0
    summary_append "- FAIL: trace excerpt missing"
fi

rmmod demo_regmap > /dev/null 2>&1 || true
capture_modules
scan_dmesg_tail
emit_env_and_files
