#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:-eth0}
PEER_IP=${2:-}
SECONDS=${3:-10}
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

if [[ -z "$PEER_IP" ]]; then
    echo "usage: $0 <ifname> <peer-ip> [seconds]" >&2
    exit 1
fi
if ! command -v iperf3 >/dev/null 2>&1; then
    echo "iperf3 not found" >&2
    exit 1
fi

TS=$(now_ts)
OUT="records/${TS}-iperf3"
ensure_dir "$OUT"

ip -s link show dev "$IFNAME" > "$OUT/ip_link_before.txt" || true
ethtool -S "$IFNAME" > "$OUT/ethtool_S_before.txt" || true

TF=$(tracefs_dir || true)
if [[ -n "${TF:-}" ]]; then
    echo 0 > "$TF/tracing_on" || true
    echo nop > "$TF/current_tracer" || true
    : > "$TF/trace" || true
    "$SCRIPT_DIR/prepare_trace_filter.sh" || true
    echo function > "$TF/current_tracer" || true
    echo 1 > "$TF/tracing_on" || true
fi

iperf3 -c "$PEER_IP" -t "$SECONDS" > "$OUT/iperf3.txt" 2>&1 || true

if [[ -n "${TF:-}" ]]; then
    echo 0 > "$TF/tracing_on" || true
    cat "$TF/trace" > "$OUT/trace.txt" || true
fi

ip -s link show dev "$IFNAME" > "$OUT/ip_link_after.txt" || true
ethtool -S "$IFNAME" > "$OUT/ethtool_S_after.txt" || true
dmesg > "$OUT/dmesg.txt" || true

cp docs/06_REPORT_TEMPLATE.md "$OUT/SUMMARY.md" || true
echo "$OUT"
