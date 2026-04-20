#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# offload_check.sh — 验证 stage13 offload 能力

set -euo pipefail

DIR=${1:-.}
IFNAME=${IFNAME:-nds13s}
ETHTOOL=${ETHTOOL:-ethtool}

mkdir -p "$DIR"

command -v "$ETHTOOL" >/dev/null 2>&1 || {
    echo "offload_check: missing ethtool" >&2
    exit 1
}

echo "[offload_check] Testing $IFNAME offload capabilities"

# 1. ethtool -k (show offload features)
echo "[offload_check] Testing ethtool -k..."
$ETHTOOL -k "$IFNAME" > "$DIR/offload_k.txt" 2>&1
grep -q "rx-checksum" "$DIR/offload_k.txt" || {
    echo "FAIL: ethtool -k missing rx-checksum" >&2
    exit 1
}
grep -q "tx-checksum" "$DIR/offload_k.txt" || {
    echo "FAIL: ethtool -k missing tx-checksum" >&2
    exit 1
}
grep -q "scatter-gather" "$DIR/offload_k.txt" || {
    echo "FAIL: ethtool -k missing scatter-gather" >&2
    exit 1
}
grep -q "generic-segmentation-offload" "$DIR/offload_k.txt" || {
    echo "FAIL: ethtool -k missing gso" >&2
    exit 1
}
grep -q "generic-receive-offload" "$DIR/offload_k.txt" || {
    echo "FAIL: ethtool -k missing gro" >&2
    exit 1
}

# 2. 检查 debugfs offload 文件
echo "[offload_check] Testing debugfs offload..."
if [[ -d "/sys/kernel/debug/netdev_stage13_soft" ]]; then
    cat /sys/kernel/debug/netdev_stage13_soft/offload > "$DIR/offload_debugfs.txt" 2>&1 || true
fi

echo ""
echo "========================================="
echo "offload_check PASSED"
echo "========================================="
echo "Files generated:"
echo "  $DIR/offload_k.txt        (ethtool -k output)"
echo "  $DIR/offload_debugfs.txt  (debugfs offload)"
echo "========================================="