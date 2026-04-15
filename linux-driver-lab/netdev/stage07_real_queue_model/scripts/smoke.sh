#!/usr/bin/env bash
# ================================================================================
# stage07 smoke.sh — 端到端 smoke 测试脚本
#
# 【功能】
# 验证 stage07 的 TX/RX 闭环路径：
#   userspace send → netdev ndo_start_xmit → stage07_kick_device →
#   NAPI poll → netif_receive_skb → userspace recv
#
# 【测试流程】
#   1. 检查 debugfs 目录（确认模块已加载）
#   2. 创建设备 UP（确保可收发）
#   3. 启动后台 recv 进程（监听 ETHERTYPE 帧）
#   4. 发送 ETHERTYPE=0x88B7 的测试帧
#   5. 等待 recv 完成
#   6. 收集 stats / queues / dmesg
#
# 【环境变量】
# - IFNAME           : 设备名（默认 nds7）
# - ETHERTYPE       : EtherType，用于过滤测试帧（默认 0x88B7）
# - SMOKE_COUNT     : 发送帧数（默认 32）
# - SMOKE_TIMEOUT_SEC: recv 超时秒数（默认 5）
#
# 【输出】
# - records/<STAMP>-stage07-smoke/
#   - ip_link_before.txt  : 发送前设备状态
#   - ip_link_after.txt   : 发送后设备状态
#   - send.txt            : 发送命令输出
#   - recv.txt            : 接收命令输出
#   - debugfs_stats.txt   : 所有计数器
#   - debugfs_queues.txt  : TX/RX queue dump
#   - dmesg_tail.txt      : 最近 120 行 dmesg
#
# 【验证要点】
# - TX: tx_submit_count == tx_complete_count（TX 无泄漏）
# - RX: rx_consume_count > 0（RX 有消费）
# - NAPI: napi_poll_count > 0（NAPI 被触发）
# - queues: 所有 TX slots FREE，所有 RX slots POSTED
# ================================================================================

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$ROOT_DIR/tools"

# 参数默认值
IFNAME=${IFNAME:-nds7}
ETHERTYPE=${ETHERTYPE:-0x88B7}
SMOKE_COUNT=${SMOKE_COUNT:-32}
SMOKE_TIMEOUT_SEC=${SMOKE_TIMEOUT_SEC:-5}
TIMEOUT_BIN=${TIMEOUT_BIN:-timeout}

# 日志目录: records/<时间戳>-stage07-smoke/
STAMP=$(date +%Y%m%d-%H%M%S)
LOG_DIR="$ROOT_DIR/records/$STAMP-stage07-smoke"
SEND_TOOL="$TOOLS_DIR/send_stage07_frame"
RECV_TOOL="$TOOLS_DIR/recv_stage07_frame"

mkdir -p "$LOG_DIR"

# ========== 前置检查 ==========

# 1. 检查 send/recv 工具是否已编译
test -x "$SEND_TOOL" || { echo "[stage07] missing $SEND_TOOL, run scripts/build.sh first" >&2; exit 1; }
test -x "$RECV_TOOL" || { echo "[stage07] missing $RECV_TOOL, run scripts/build.sh first" >&2; exit 1; }

# 2. 检查模块是否已加载（lsmod 有输出则已加载）
lsmod | awk '{print $1}' | grep -qx netdev_stage07 || { echo "[stage07] module netdev_stage07 is not loaded" >&2; exit 1; }

# ========== 测试执行 ==========

# 1. 确保设备 UP
sudo ip link set dev "$IFNAME" up

# 2. 发送前设备状态（用于对比）
ip -details link show "$IFNAME" | tee "$LOG_DIR/ip_link_before.txt"

# 3. 启动后台 recv 进程
#    timeout 命令确保 recv 不会永久阻塞
#    - & 放到后台运行
#    - RECV_PID 保存进程 ID，用于后续 wait
sudo "$TIMEOUT_BIN" "$SMOKE_TIMEOUT_SEC" \
    "$RECV_TOOL" "$IFNAME" "$ETHERTYPE" "$SMOKE_COUNT" "$SMOKE_TIMEOUT_SEC" \
    > "$LOG_DIR/recv.txt" 2>&1 &
RECV_PID=$!

# 4. 等待 recv 启动完成（1 秒足够了）
sleep 1

# 5. 发送测试帧
#    send_stage07_frame 参数:
#    $1 = ifname        : 设备名
#    $2 = payload      : 测试数据内容
#    $3 = ethertype    : EtherType（十六进制）
#    $4 = count        : 发送帧数
#    $5 = flags        : 标志位（0 = 普通发送）
sudo "$SEND_TOOL" "$IFNAME" "hello_stage07_queue" "$ETHERTYPE" "$SMOKE_COUNT" 0 \
    | tee "$LOG_DIR/send.txt"

# 6. 等待 recv 进程结束（wait 会返回 exit code）
wait "$RECV_PID" || true

# 7. 等待所有包处理完成
sleep 1

# ========== 收集结果 ==========

# 8. 发送后设备统计（TX/RX bytes/packets 对比）
ip -s link show "$IFNAME" | tee "$LOG_DIR/ip_link_after.txt"

# 9. debugfs 统计（所有计数器）
if [[ -f /sys/kernel/debug/netdev_stage07/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage07/stats | tee "$LOG_DIR/debugfs_stats.txt"
fi

# 10. debugfs queue dump（TX/RX slot 状态）
if [[ -f /sys/kernel/debug/netdev_stage07/queues ]]; then
    sudo cat /sys/kernel/debug/netdev_stage07/queues | tee "$LOG_DIR/debugfs_queues.txt"
fi

# 11. dmesg tail（最近 120 行）
sudo dmesg | tail -n 120 | tee "$LOG_DIR/dmesg_tail.txt" >/dev/null

echo "[stage07] smoke record -> $LOG_DIR"
