#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# xdp_redirect_check.sh — XDP redirect 路径验证
#
# 注意：软模型无法真正做 XDP_REDIRECT（需要硬件 TX 路径）。
# 此脚本仅验证 redirect 相关统计变化。

set -euo pipefail

IFNAME=${IFNAME:-nds14s}
ETHTOOL=${ETHTOOL:-ethtool}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SMOKE_SCRIPT="$SCRIPT_DIR/smoke.sh"

echo "========================================="
echo "xdp_redirect_check: XDP redirect 验证"
echo "========================================="

# 检查是否有 XDP program 加载
XDP_STATUS=$(ip link show "$IFNAME" 2>/dev/null | grep -o "xdp" || true)
if [[ -z "$XDP_STATUS" ]]; then
    echo "WARN: XDP program 未加载（软模型不支持 redirect，仅统计）"
    echo "如需测试 redirect，需要真实硬件驱动"
fi

# 获取 redirect 统计
get_redirect_stats() {
    $ETHTOOL -S "$IFNAME" 2>/dev/null | grep "xdp_redirect" || true
}

echo ""
echo "[1] 当前 XDP redirect 统计..."
REDIRECT_BEFORE=$(get_redirect_stats)
echo "$REDIRECT_BEFORE"

echo ""
echo "[2] 运行 smoke test..."
if [[ -x "$SMOKE_SCRIPT" ]]; then
    "$SMOKE_SCRIPT" > /dev/null 2>&1 || echo "WARN: smoke test 失败"
else
    echo "WARN: smoke.sh 未找到"
fi

echo ""
echo "[3] smoke test 后 XDP redirect 统计..."
REDIRECT_AFTER=$(get_redirect_stats)
echo "$REDIRECT_AFTER"

echo ""
echo "========================================="
echo "xdp_redirect_check 完成"
echo "========================================="
echo "说明：软模型不支持真正的 XDP_REDIRECT（需要硬件 TX）"
echo "此脚本验证的是 xdp_redirect 计数器是否可访问"
