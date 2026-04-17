#!/usr/bin/env bash
#
# smoke.sh — stage08 完整验证脚本
#
# 【学习要点】
#
# 1. 脚本结构
#    - 收集测试前状态（before snapshot）
#    - 并发执行接收和发送
#    - 收集测试后状态（after snapshot）
#    - 生成 SMOKE_REPORT.md
#
# 2. 并发模型
#    - recv 先启动（后台运行）
#    - 等待 1 秒确保 recv 开始监听
#    - send 同步发送
#    - wait 等待 recv 完成
#
# 3. debugfs 观测点
#    - /sys/kernel/debug/netdev_stage08/stats      — 完整统计
#    - /sys/kernel/debug/netdev_stage08/queues    — TX/RX ring 状态
#    - /sys/kernel/debug/netdev_stage08/timeline  — 各阶段时间戳
#
# 4. smoke vs smoke_test
#    - smoke：快速验证（发送少量帧）
#    - smoke_test：完整测试（更多帧，更长时间）
#

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$ROOT_DIR/tools"

IFNAME=${IFNAME:-nds8}
ETHERTYPE=${ETHERTYPE:-0x88B8}
SMOKE_COUNT=${SMOKE_COUNT:-32}
SMOKE_TIMEOUT_SEC=${SMOKE_TIMEOUT_SEC:-5}
TIMEOUT_BIN=${TIMEOUT_BIN:-timeout}

STAMP=$(date +%Y%m%d-%H%M%S)
LOG_DIR="$ROOT_DIR/records/$STAMP-stage08-smoke"
SEND_TOOL="$TOOLS_DIR/send_stage08_frame"
RECV_TOOL="$TOOLS_DIR/recv_stage08_frame"

mkdir -p "$LOG_DIR"

# 【学习】工具可用性检查
# 确保编译产物存在
test -x "$SEND_TOOL" || { echo "[stage08] missing $SEND_TOOL, run scripts/build.sh first" >&2; exit 1; }
test -x "$RECV_TOOL" || { echo "[stage08] missing $RECV_TOOL, run scripts/build.sh first" >&2; exit 1; }

# 【学习】模块加载检查
# lsmod | awk '{print $1}' | grep -qx <name>
# - lsmod：列出已加载模块
# - awk '{print $1}'：取第一列（模块名）
# - grep -qx：精确匹配（无需正则）
lsmod | awk '{print $1}' | grep -qx netdev_stage08 || { echo "[stage08] module netdev_stage08 is not loaded" >&2; exit 1; }

# 【学习】接口 UP
# 网卡必须 UP 才能收发帧
# ip link set dev <ifname> up
sudo ip link set dev "$IFNAME" up

# 【学习】测试前快照
# 记录测试前的接口状态、统计、timeline
ip -details link show "$IFNAME" | tee "$LOG_DIR/ip_link_before.txt"

if [[ -f /sys/kernel/debug/netdev_stage08/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage08/stats | tee "$LOG_DIR/debugfs_stats_before.txt" >/dev/null
fi
if [[ -f /sys/kernel/debug/netdev_stage08/timeline ]]; then
    sudo cat /sys/kernel/debug/netdev_stage08/timeline | tee "$LOG_DIR/debugfs_timeline_before.txt" >/dev/null
fi

#
# 【学习】并发收发
#
# 模型：
#   recv 进程          send 进程
#      |                  |
#   recvfrom() 阻塞       |
#      |             sendto() 发送
#   recv 32帧             |
#      |                  |
#   超时退出               |
#
# recv 设置 PACKET_IGNORE_OUTGOING，不会收到自己发出的帧
# 但会收到驱动环回来的帧
#
# 关键点：
# 1. recv 先启动，等待 1 秒确保它开始监听
# 2. send 发送 32 帧
# 3. recv 在 SMOKE_TIMEOUT_SEC (5秒) 内最多接收 32 帧
# 4. 驱动收到帧后，会通过 backend worker 异步处理并环回
#

sudo "$TIMEOUT_BIN" "$SMOKE_TIMEOUT_SEC" \
    "$RECV_TOOL" "$IFNAME" "$ETHERTYPE" "$SMOKE_COUNT" "$SMOKE_TIMEOUT_SEC" \
    > "$LOG_DIR/recv.txt" 2>&1 &
REC_PID=$!

sleep 1

sudo "$SEND_TOOL" "$IFNAME" "hello_stage08_async_backend" "$ETHERTYPE" "$SMOKE_COUNT" 0 \
    | tee "$LOG_DIR/send.txt"

# 【学习】wait 等待后台进程
# 如果 recv 超时退出，wait 会捕获到 exit code
wait "$REC_PID" || true
sleep 1

# 【学习】测试后快照
# 与测试前对比，可观察变化
ip -s link show "$IFNAME" | tee "$LOG_DIR/ip_link_after.txt"

if [[ -f /sys/kernel/debug/netdev_stage08/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage08/stats | tee "$LOG_DIR/debugfs_stats_after.txt"
fi
if [[ -f /sys/kernel/debug/netdev_stage08/queues ]]; then
    sudo cat /sys/kernel/debug/netdev_stage08/queues | tee "$LOG_DIR/debugfs_queues_after.txt"
fi
if [[ -f /sys/kernel/debug/netdev_stage08/timeline ]]; then
    sudo cat /sys/kernel/debug/netdev_stage08/timeline | tee "$LOG_DIR/debugfs_timeline_after.txt"
fi

# 【学习】dmesg 日志
# 内核日志包含驱动的 printk 输出
# tail -n 160 取最近 160 行（避免太多）
sudo dmesg | tail -n 160 | tee "$LOG_DIR/dmesg_tail.txt" >/dev/null

# 【学习】生成 smoke 报告
cat > "$LOG_DIR/SMOKE_REPORT.md" <<EOF
# stage08 smoke report

- ifname: $IFNAME
- ethertype: $ETHERTYPE
- count: $SMOKE_COUNT
- timeout_sec: $SMOKE_TIMEOUT_SEC

本次 smoke 的重点不是极限性能，而是确认：

1. send -> submit
2. submit -> doorbell
3. doorbell -> backend worker
4. backend worker -> irq
5. irq -> napi poll
6. poll -> complete / consume / refill

请结合：
- debugfs_stats_after.txt
- debugfs_timeline_after.txt
- debugfs_queues_after.txt
- recv.txt
综合判断。
EOF

echo "[stage08] smoke record -> $LOG_DIR"