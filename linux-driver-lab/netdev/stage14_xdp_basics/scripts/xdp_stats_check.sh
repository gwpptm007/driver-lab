#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# xdp_stats_check.sh — 验证 XDP 统计

set -euo pipefail

IFNAME=${IFNAME:-nds14s}
ETHTOOL=${ETHTOOL:-ethtool}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SMOKE_SCRIPT="$SCRIPT_DIR/smoke.sh"

echo "========================================="
echo "xdp_stats_check: XDP 统计验证"
echo "========================================="

# 读取 XDP 统计的函数
get_xdp_stats() {
    $ETHTOOL -S "$IFNAME" 2>/dev/null | grep -E "xdp_pass|xdp_drop|xdp_tx|xdp_redirect|xdp_err" || true
}

# 1. 检查当前 XDP 状态（无 program）
echo ""
echo "[1] XDP 初始状态（无 program）..."
XDP_INIT=$(get_xdp_stats)
echo "$XDP_INIT"

# 2. 运行 smoke test
echo ""
echo "[2] 运行 smoke test..."
if [[ -x "$SMOKE_SCRIPT" ]]; then
    "$SMOKE_SCRIPT" > /dev/null 2>&1 || echo "WARN: smoke test 失败"
else
    echo "WARN: smoke.sh 未找到"
fi

# 3. 再次检查 XDP 统计
echo ""
echo "[3] Smoke test 后 XDP 统计..."
XDP_AFTER=$(get_xdp_stats)
echo "$XDP_AFTER"

# 4. 对比 debugfs
echo ""
echo "[4] debugfs XDP 状态..."
if [[ -d "/sys/kernel/debug/netdev_stage14_soft" ]]; then
    cat /sys/kernel/debug/netdev_stage14_soft/xdp 2>&1 || echo "WARN: 无法读取 debugfs xdp"
fi

echo ""
echo "========================================="
echo "xdp_stats_check 完成"
echo "========================================="
