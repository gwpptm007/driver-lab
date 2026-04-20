#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# ethtool_check.sh — 验证 stage12 ethtool/control plane 是否可用

set -euo pipefail

DIR=${1:-.}
MODE=${2:-query}
IFNAME=${IFNAME:-nds12s}
ETHTOOL=${ETHTOOL:-ethtool}

mkdir -p "$DIR"

command -v "$ETHTOOL" >/dev/null 2>&1 || {
    echo "ethtool_check: missing ethtool" >&2
    exit 1
}

echo "[ethtool_check] Testing $IFNAME in mode=$MODE"

# 1. ethtool -i (驱动信息)
echo "[ethtool_check] Testing ethtool -i..."
$ETHTOOL -i "$IFNAME" > "$DIR/ethtool_i.txt" 2>&1
grep -q "driver: netdev_stage12" "$DIR/ethtool_i.txt" || {
    echo "FAIL: ethtool -i missing driver name" >&2
    exit 1
}

# 2. ethtool -S (统计)
echo "[ethtool_check] Testing ethtool -S..."
$ETHTOOL -S "$IFNAME" > "$DIR/ethtool_S.txt" 2>&1
# 检查关键统计项
grep -q "tx_packets:" "$DIR/ethtool_S.txt" || {
    echo "FAIL: ethtool -S missing tx_packets" >&2
    exit 1
}
grep -q "rx_consume_count:" "$DIR/ethtool_S.txt" || {
    echo "FAIL: ethtool -S missing rx_consume_count" >&2
    exit 1
}

# 3. ethtool -g (ringparam)
echo "[ethtool_check] Testing ethtool -g..."
$ETHTOOL -g "$IFNAME" > "$DIR/ethtool_g.txt" 2>&1
grep -q "RX:" "$DIR/ethtool_g.txt" || {
    echo "FAIL: ethtool -g missing RX info" >&2
    exit 1
}

# 4. ethtool -l (channels)
echo "[ethtool_check] Testing ethtool -l..."
$ETHTOOL -l "$IFNAME" > "$DIR/ethtool_l.txt" 2>&1
grep -q "Combined:" "$DIR/ethtool_l.txt" || {
    echo "FAIL: ethtool -l missing Combined info" >&2
    exit 1
}

# 5. 额外：检查是否支持 priv_flags
echo "[ethtool_check] Checking priv_flags support..."
if $ETHTOOL --show-priv-flags "$IFNAME" > "$DIR/ethtool_priv_flags.txt" 2>&1; then
    echo "[ethtool_check] priv_flags supported"
else
    echo "[ethtool_check] priv_flags not supported (optional)"
fi

if [[ "$MODE" == "exercise-channels" ]]; then
    echo "[ethtool_check] Testing set_channels..."

    # 读取当前 channel 设置
    current=$($ETHTOOL -l "$IFNAME" 2>/dev/null | awk '/Current hardware settings:/ {f=1;next} f && /Combined:/ {print $2; exit}')
    [[ -n "$current" ]] || {
        echo "FAIL: failed to parse current combined" >&2
        exit 1
    }
    echo "[ethtool_check] Current combined channels: $current"

    # 尝试设置为 1（如果当前不是1）
    if [[ "$current" != "1" ]]; then
        echo "[ethtool_check] Testing: set combined=1..."
        sudo ip link set "$IFNAME" down 2>/dev/null || true
        $ETHTOOL -L "$IFNAME" combined 1 2>/dev/null || {
            echo "FAIL: set_channels combined 1 failed" >&2
            sudo ip link set "$IFNAME" up 2>/dev/null || true
            exit 1
        }
        $ETHTOOL -l "$IFNAME" > "$DIR/ethtool_l_after_set1.txt"
        grep -qE 'Combined:[[:space:]]+1$' "$DIR/ethtool_l_after_set1.txt" || {
            echo "FAIL: set_channels combined 1 did not take effect" >&2
            sudo ip link set "$IFNAME" up 2>/dev/null || true
            exit 1
        }
        echo "[ethtool_check] Restore: set combined=$current..."
        $ETHTOOL -L "$IFNAME" combined "$current" 2>/dev/null || true
        sudo ip link set "$IFNAME" up 2>/dev/null || true
    else
        echo "[ethtool_check] Already at combined=1, skip channel change test"
    fi
fi

echo ""
echo "========================================="
echo "ethtool_check PASSED"
echo "========================================="
echo "Files generated:"
echo "  $DIR/ethtool_i.txt    (drvinfo)"
echo "  $DIR/ethtool_S.txt    (stats)"
echo "  $DIR/ethtool_g.txt    (ringparam)"
echo "  $DIR/ethtool_l.txt    (channels)"
echo "========================================="