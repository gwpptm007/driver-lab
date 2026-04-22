#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# xdp_check.sh — 验证 stage14 XDP 功能

set -euo pipefail

IFNAME=${IFNAME:-nds14s}
ETHTOOL=${ETHTOOL:-ethtool}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SMOKE_SCRIPT="$SCRIPT_DIR/smoke.sh"

echo "========================================="
echo "xdp_check: XDP 功能验证"
echo "========================================="

# 1. 检查接口是否存在
echo ""
echo "[1] 检查接口 $IFNAME 是否存在..."
if ! ip link show dev "$IFNAME" >/dev/null 2>&1; then
    echo "FAIL: 设备 $IFNAME 不存在" >&2
    exit 1
fi
echo "✓ 接口 $IFNAME 存在"

# 2. 检查 XDP debugfs 目录
echo ""
echo "[2] 检查 debugfs XDP 状态..."
if [[ -d "/sys/kernel/debug/netdev_stage14_soft" ]]; then
    cat /sys/kernel/debug/netdev_stage14_soft/xdp > /tmp/xdp_debugfs_before.txt 2>&1 || true
    echo "XDP 状态:"
    cat /tmp/xdp_debugfs_before.txt
    grep -q "xdp_prog=(nil\|0x0\|NULL)" /tmp/xdp_debugfs_before.txt && echo "✓ XDP program 未加载（初始状态正确）"
else
    echo "WARN: debugfs 目录不存在，跳过"
fi

# 3. 检查 ethtool -S 是否显示 XDP 统计
echo ""
echo "[3] 检查 ethtool -S XDP 统计..."
XDP_STATS=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep -E "xdp_pass|xdp_drop|xdp_tx|xdp_redirect" || true)
if [[ -n "$XDP_STATS" ]]; then
    echo "XDP 统计:"
    echo "$XDP_STATS"
    echo "✓ ethtool -S 显示 XDP 统计"
else
    echo "WARN: ethtool -S 无 XDP 统计（可能 XDP 未加载）"
fi

# 4. 检查 ip link show
echo ""
echo "[4] 检查 ip link show..."
IP_SHOW=$(ip link show "$IFNAME" 2>/dev/null || true)
echo "$IP_SHOW"

# 5. smoke test
echo ""
echo "[5] Smoke test..."
if [[ -x "$SMOKE_SCRIPT" ]]; then
    "$SMOKE_SCRIPT" || echo "WARN: smoke test 失败"
else
    echo "WARN: smoke.sh 未找到"
fi

echo ""
echo "========================================="
echo "xdp_check 完成"
echo "========================================="
echo ""
echo "手动验证步骤："
echo "1. 加载 XDP program:"
echo "   ip link set dev $IFNAME xdp obj xdp_count.o sec test"
echo "2. 查看 ip link show:"
echo "   ip link show $IFNAME  (应该显示 xdp 标志)"
echo "3. 运行 smoke test 后查看 xdp_pass 增长:"
echo "   ethtool -S $IFNAME | grep xdp_pass"
echo "4. 卸载 XDP program:"
echo "   ip link set dev $IFNAME xdp off"
