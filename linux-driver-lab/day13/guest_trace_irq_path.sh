#!/bin/sh
set -eu

# guest_trace_irq_path.sh
# -----------------------
# 这个脚本在 guest 内完成一轮“最小、可重复、可归档”的 function_graph 实验。
#
# 它会自动做这些事：
# 1. 检查 debugfs/tracing 是否可用
# 2. 检查 function_graph tracer 是否存在
# 3. 清理旧 tracer、旧 buffer、旧过滤器
# 4. 写入本轮最关心的函数集合
# 5. 打开 function_graph
# 6. 触发一次 demo_regmap/trigger
# 7. 保存 trace 文本与前后 snapshot
#
# 用户最常用的调用方式：
#   /bin/day13_trace_irq_path.sh 1
#
# 这比手工一条条 echo 到 tracing 目录更适合教学：
# - 步骤固定
# - 输出可复现
# - 留档方便

TR=/sys/kernel/debug/tracing
DBG=/sys/kernel/debug/demo_regmap
TARGETS=/etc/day13_function_graph_targets.txt
TIMES="${1:-1}"
TRACE_OUT=/tmp/day13_irq_function_graph.txt
SNAP_BEFORE=/tmp/day13_snapshot_before.txt
SNAP_AFTER=/tmp/day13_snapshot_after.txt
META_OUT=/tmp/day13_trace_meta.txt

if [ ! -d /sys/kernel/debug ]; then
    mkdir -p /sys/kernel/debug
fi

if ! mount | grep -q "on /sys/kernel/debug type debugfs"; then
    mount -t debugfs debugfs /sys/kernel/debug || true
fi

if [ ! -d "$TR" ]; then
    echo "[ERROR] tracing dir not found: $TR"
    exit 1
fi

if [ ! -d "$DBG" ]; then
    echo "[ERROR] demo_regmap debugfs dir not found: $DBG"
    echo "[HINT ] 先 insmod /demo_regmap.ko"
    exit 1
fi

if ! grep -wq function_graph "$TR/available_tracers"; then
    echo "[ERROR] function_graph not available"
    echo "[HINT ] 检查内核 ftrace 配置"
    exit 1
fi

cat "$DBG/snapshot" > "$SNAP_BEFORE"

echo 0 > "$TR/tracing_on"
echo nop > "$TR/current_tracer"
: > "$TR/trace"
: > "$TR/set_graph_function"

if [ -f "$TARGETS" ]; then
    while IFS= read -r fn; do
        case "$fn" in
            ''|'#'*)
                continue
                ;;
        esac
        echo "$fn" >> "$TR/set_graph_function" 2>/dev/null || true
    done < "$TARGETS"
fi

echo function_graph > "$TR/current_tracer"

{
    echo "times=$TIMES"
    echo "date=$(date)"
    echo "tracer=$(cat "$TR/current_tracer")"
    echo "available_tracers=$(cat "$TR/available_tracers")"
    echo "----- set_graph_function -----"
    cat "$TR/set_graph_function"
    echo "----- snapshot_before -----"
    cat "$SNAP_BEFORE"
} > "$META_OUT"

echo 1 > "$TR/tracing_on"
echo "$TIMES" > "$DBG/trigger"
sleep 1
echo 0 > "$TR/tracing_on"

cat "$TR/trace" > "$TRACE_OUT"
cat "$DBG/snapshot" > "$SNAP_AFTER"

TRACE_LINES=$(wc -l < "$TRACE_OUT" 2>/dev/null || echo 0)

echo "[ OK  ] function_graph trace saved: $TRACE_OUT"
echo "[ OK  ] snapshot before saved   : $SNAP_BEFORE"
echo "[ OK  ] snapshot after saved    : $SNAP_AFTER"
echo "[ OK  ] trace meta saved        : $META_OUT"
echo "[ INFO] trace lines             : $TRACE_LINES"
echo "[ INFO] next step               : cat $TRACE_OUT"
