#!/bin/sh
set -eu

# guest_archive_trace.sh
# ----------------------
# 这个脚本把 day13 一轮实验中最关键的文字材料打包到 /tmp/day13-archive-时间戳 目录里。
#
# 归档的意义：
# - 后面写 README/学习笔记时不用回头翻控制台
# - 截图只是“图”，而 trace/snapshot/meta/dmesg 是原始文字证据
# - 便于和 day12 的 snapshot、warning、trace 分析做对照

TR=/sys/kernel/debug/tracing
DBG=/sys/kernel/debug/demo_regmap
STAMP=$(date +%Y%m%d-%H%M%S)
OUTDIR="/tmp/day13-archive-$STAMP"

mkdir -p "$OUTDIR"

[ -f /tmp/day13_irq_function_graph.txt ] && cp /tmp/day13_irq_function_graph.txt "$OUTDIR/" || true
[ -f /tmp/day13_snapshot_before.txt ] && cp /tmp/day13_snapshot_before.txt "$OUTDIR/" || true
[ -f /tmp/day13_snapshot_after.txt ] && cp /tmp/day13_snapshot_after.txt "$OUTDIR/" || true
[ -f /tmp/day13_trace_meta.txt ] && cp /tmp/day13_trace_meta.txt "$OUTDIR/" || true

if [ -d "$TR" ]; then
    cat "$TR/current_tracer" > "$OUTDIR/current_tracer.txt" 2>/dev/null || true
    cat "$TR/set_graph_function" > "$OUTDIR/set_graph_function.txt" 2>/dev/null || true
fi

if [ -d "$DBG" ]; then
    cat "$DBG/snapshot" > "$OUTDIR/snapshot_now.txt" 2>/dev/null || true
fi

dmesg | tail -n 200 > "$OUTDIR/dmesg_tail.txt" 2>/dev/null || true
uname -a > "$OUTDIR/uname.txt" 2>/dev/null || true

cat > "$OUTDIR/SCREENSHOT_TODO.txt" <<EOT
Day13 建议截图清单：
1. insmod /demo_regmap.ko 后，cat /sys/kernel/debug/demo_regmap/snapshot 的正常输出
2. 执行 /bin/day13_trace_irq_path.sh 1 后，脚本提示 trace/snapshot 保存成功
3. cat /tmp/day13_irq_function_graph.txt 时，包含下面关键链路的区域：
   demo_regmap_trigger_write
   generic_handle_irq
   handle_fasteoi_irq
   handle_irq_event
   __handle_irq_event_percpu
   demo_regmap_handler
4. 同一份 trace 中，出现 worker_thread / process_one_work / demo_regmap_workfn 的区域
5. 可选：cat $OUTDIR/snapshot_now.txt 作为 trace 后状态留档
EOT

echo "[ OK  ] archive created: $OUTDIR"
echo "[ INFO] screenshot checklist: $OUTDIR/SCREENSHOT_TODO.txt"
