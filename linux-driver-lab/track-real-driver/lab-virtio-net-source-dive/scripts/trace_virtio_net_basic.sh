#!/usr/bin/env bash
set -euo pipefail

TRACE_DIR=${1:-/sys/kernel/debug/tracing}
EVENT_OUT=${2:-records/manual-source-dive/trace_basic.txt}
mkdir -p "$(dirname "$EVENT_OUT")"

if [[ ! -d "$TRACE_DIR" ]]; then
  echo "trace dir not found: $TRACE_DIR" >&2
  exit 1
fi

echo 0 > "$TRACE_DIR/tracing_on"
: > "$TRACE_DIR/trace"

for f in virtnet_poll start_xmit virtqueue_kick; do
  grep -q "$f" "$TRACE_DIR/available_filter_functions" 2>/dev/null || true
done

echo function > "$TRACE_DIR/current_tracer" || true

for f in virtnet_poll start_xmit virtqueue_kick; do
  echo "$f" >> "$TRACE_DIR/set_ftrace_filter" 2>/dev/null || true
done

echo 1 > "$TRACE_DIR/tracing_on"
sleep 3
echo 0 > "$TRACE_DIR/tracing_on"

cat "$TRACE_DIR/trace" > "$EVENT_OUT"
echo "$EVENT_OUT"
