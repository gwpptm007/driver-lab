#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# offload_experiment.sh — ethtool -K 开关实验，验证 GRO 路径差异

set -euo pipefail

IFNAME=${IFNAME:-nds13s}
ETHTOOL=${ETHTOOL:-ethtool}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SMOKE_SCRIPT="$SCRIPT_DIR/smoke.sh"

command -v "$ETHTOOL" >/dev/null 2>&1 || {
    echo "offload_experiment: missing ethtool" >&2
    exit 1
}

echo "========================================="
echo "offload_experiment: GRO 开关实验"
echo "========================================="

# 实验1: GRO off
echo ""
echo "[实验1] GRO 关闭状态测试"
echo "-----------------------------------"
ip link set "$IFNAME" down 2>/dev/null || true
$ETHTOOL -K "$IFNAME" gro off 2>/dev/null || true
ip link set "$IFNAME" up 2>/dev/null || true
sleep 1

gro_before=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep "rx_gro_packets:" | awk '{print $2}')
echo "测试前 rx_gro_packets: $gro_before"

if [[ -x "$SMOKE_SCRIPT" ]]; then
    "$SMOKE_SCRIPT" > /dev/null 2>&1 || true
else
    echo "smoke.sh not found, skipping"
fi

sleep 1
gro_after=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep "rx_gro_packets:" | awk '{print $2}')
echo "测试后 rx_gro_packets: $gro_after"

if [[ "$gro_after" == "$gro_before" ]] || [[ -z "$gro_after" ]]; then
    echo "✓ GRO off: rx_gro_packets 未增长（符合预期）"
else
    echo "✗ GRO off: rx_gro_packets 异常增长"
fi

# 实验2: GRO on
echo ""
echo "[实验2] GRO 开启状态测试"
echo "-----------------------------------"
ip link set "$IFNAME" down 2>/dev/null || true
$ETHTOOL -K "$IFNAME" gro on 2>/dev/null || true
ip link set "$IFNAME" up 2>/dev/null || true
sleep 1

gro_before=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep "rx_gro_packets:" | awk '{print $2}')
echo "测试前 rx_gro_packets: $gro_before"

if [[ -x "$SMOKE_SCRIPT" ]]; then
    "$SMOKE_SCRIPT" > /dev/null 2>&1 || true
else
    echo "smoke.sh not found, skipping"
fi

sleep 1
gro_after=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep "rx_gro_packets:" | awk '{print $2}')
echo "测试后 rx_gro_packets: $gro_after"

if [[ -n "$gro_after" ]] && [[ "$gro_after" != "$gro_before" ]]; then
    echo "✓ GRO on: rx_gro_packets 增长（符合预期）"
else
    echo "✗ GRO on: rx_gro_packets 未增长"
fi

# 实验3: feature_set_count 验证
echo ""
echo "[实验3] feature_set_count 验证"
echo "-----------------------------------"
feature_before=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep "feature_set_count:" | awk '{print $2}')
echo "切换前 feature_set_count: $feature_before"

$ETHTOOL -K "$IFNAME" gro off 2>/dev/null || true
sleep 1

feature_after=$($ETHTOOL -S "$IFNAME" 2>/dev/null | grep "feature_set_count:" | awk '{print $2}')
echo "切换后 feature_set_count: $feature_after"

if [[ -n "$feature_after" ]] && [[ "$feature_after" != "$feature_before" ]]; then
    echo "✓ feature_set_count++（ndo_set_features 被调用）"
else
    echo "✗ feature_set_count 未变化"
fi

echo ""
echo "========================================="
echo "offload_experiment 完成"
echo "========================================="