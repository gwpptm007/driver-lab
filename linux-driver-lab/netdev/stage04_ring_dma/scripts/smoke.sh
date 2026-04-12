#!/usr/bin/env bash
#
# smoke.sh — stage04_ring_dma 自动化 smoke test
#
# 【测试流程】
#   1. 检查 userspace 工具和模块是否就绪
#   2. 启动 recv_stage04_frame（后台，timeout 保护）
#   3. 发送 burst 帧（send_stage04_frame）
#   4. 收集 ip link / debugfs stats / debugfs rings 输出
#
# 【环境变量】
#   IFNAME              → 接口名（默认 nds4）
#   ETHERTYPE           → ethertype（默认 0x88B7）
#   SMOKE_COUNT         → burst 数量（默认 32）
#   SMOKE_TIMEOUT_SEC   → recv 超时（默认 5 秒）
#
# 【smoke 成功标志】
#   - recv_tool 输出中有 "len=xx protocol=0x88b7"
#   - debugfs_stats 中 rx_packets > 0, tx_packets > 0
#   - debugfs_rings 中 TX/RX ring 状态可见

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
IFNAME=${IFNAME:-nds4}
ETHERTYPE=${ETHERTYPE:-0x88B7}
SMOKE_COUNT=${SMOKE_COUNT:-32}
SMOKE_TIMEOUT_SEC=${SMOKE_TIMEOUT_SEC:-5}
TIMEOUT_BIN=${TIMEOUT_BIN:-timeout}
SEND_TOOL="$ROOT_DIR/tools/send_stage04_frame"
RECV_TOOL="$ROOT_DIR/tools/recv_stage04_frame"
STAMP=$(date +%Y%m%d-%H%M%S)
LOG_DIR="$ROOT_DIR/records/$STAMP-stage04-smoke"
mkdir -p "$LOG_DIR"

if [[ ! -x "$SEND_TOOL" || ! -x "$RECV_TOOL" ]]; then
    echo "[stage04] userspace tools missing, run 'make build-userspace' first" >&2
    exit 1
fi

if ! lsmod | awk '{print $1}' | grep -qx netdev_stage04; then
    echo "[stage04] module netdev_stage04 is not loaded" >&2
    exit 1
fi

sudo ip link set dev "$IFNAME" up
ip -details link show "$IFNAME" | tee "$LOG_DIR/ip_link_before.txt"

# 启动 receiver（后台），设置 timeout 防止一直等
sudo "$TIMEOUT_BIN" "$SMOKE_TIMEOUT_SEC" \
    "$RECV_TOOL" "$IFNAME" "$ETHERTYPE" "$SMOKE_COUNT" "$SMOKE_TIMEOUT_SEC" \
    > "$LOG_DIR/recv.txt" 2>&1 &
RECV_PID=$!
sleep 1  # 确保 receiver 先启动

# 发送 burst 帧
sudo "$SEND_TOOL" "$IFNAME" "hello_stage04_burst" "$ETHERTYPE" "$SMOKE_COUNT" 0 \
    | tee "$LOG_DIR/send_tool.txt"
wait "$RECV_PID" || true
sleep 1

# 收集调试信息
ip -s link show "$IFNAME" | tee "$LOG_DIR/ip_link_after.txt"
if [[ -f /sys/kernel/debug/netdev_stage04/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage04/stats | tee "$LOG_DIR/debugfs_stats.txt"
fi
if [[ -f /sys/kernel/debug/netdev_stage04/rings ]]; then
    sudo cat /sys/kernel/debug/netdev_stage04/rings | tee "$LOG_DIR/debugfs_rings.txt"
fi

echo "[stage04] smoke record -> $LOG_DIR"
