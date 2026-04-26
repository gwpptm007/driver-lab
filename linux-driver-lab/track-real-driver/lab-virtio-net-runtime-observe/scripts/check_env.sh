#!/usr/bin/env bash
set -euo pipefail

echo "[1/6] tools"
for cmd in ip ethtool awk grep sed date uname; do
    command -v "$cmd" >/dev/null 2>&1 || echo "missing tool: $cmd"
done

echo "[2/6] tracefs"
if [[ -d /sys/kernel/tracing ]]; then
    echo "tracefs: /sys/kernel/tracing"
elif [[ -d /sys/kernel/debug/tracing ]]; then
    echo "tracefs: /sys/kernel/debug/tracing"
else
    echo "tracefs not found"
fi

echo "[3/6] iperf3(optional)"
if command -v iperf3 >/dev/null 2>&1; then
    echo "iperf3: found"
else
    echo "iperf3: not found (ping workload still usable)"
fi

echo "[4/6] kernel"
uname -a

echo "[5/6] modules"
lsmod | grep -E 'virtio_net|virtio_ring|virtio_pci' || true

echo "[6/6] done"
