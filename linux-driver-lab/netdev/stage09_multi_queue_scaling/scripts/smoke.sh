#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# smoke.sh — stage09 多队列 smoke test（发送 + 接收 + 验证）
# =============================================================================
#
# 【学习要点】
#
# 1. smoke test 的定义
#    "冒烟测试"：快速验证最基本功能是否正常，不做穷尽测试
#    核心链路：send → backend 处理 → loopback → recv
#
# 2. Before/After 差分验证
#    在发送前记录 debugfs/stats（快照 A），发送后再记录（快照 B）
#    B - A 得到增量，反映本次测试的实际发生的事件数
#    这种方法比直接读取"绝对值"更准确，排除历史累计干扰
#
# 3. 并发收发（recv 后台运行）
#    recv_stage09_frame 在后台运行（&），先启动 recv
#    sleep 0.5 等待 recv 就绪，然后发送
#    wait 等待 recv 结束
#    为什么要并发？因为 recv 不知道发送何时来，需要先监听
#
# 4. 多队列 smoke 与 stage08 的差异
#    stage09 需要验证队列分布（queue_dist_check.sh）
#    - 检查 stats 中是否有 >= 2 个队列有 tx_submit
#    - 这是多队列区别于单队列的关键证据
#
# 5. debugfs 快照
#    stats/queues/timeline 三个文件分别记录不同维度
#    stats：计数器累计值
#    queues：ring index 和 slot 状态
#    timeline：最近一次事务的时序（per-queue）
#
# 6. dmesg tail
#    dmesg | tail -n 200 只取最近 200 行，防止日志过多
#    200 行通常是足够的，能够覆盖本次测试的驱动输出
#
# 7. 生成 SMOKE_REPORT.md
#    记录测试参数，方便后续复现和对比
#
# =============================================================================

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$ROOT_DIR/tools"
IFNAME=${IFNAME:-nds9}
PROTO=${PROTO:-0x88B9}
COUNT=${COUNT:-64}
RECORD_DIR="$ROOT_DIR/records/$(date +%Y%m%d-%H%M%S)-stage09-smoke"
DBG_DIR=${DBG_DIR:-/sys/kernel/debug/netdev_stage09}
mkdir -p "$RECORD_DIR"

sudo -n true >/dev/null 2>&1 || {
    echo "[stage09] sudo -n true failed; please pre-authorize sudo" >&2
    exit 1
}

if [[ ! -x "$TOOLS_DIR/send_stage09_frame" || ! -x "$TOOLS_DIR/recv_stage09_frame" ]]; then
    echo "[stage09] tools missing, run scripts/build.sh first" >&2
    exit 1
fi

if [[ -d "$DBG_DIR" ]]; then
    sudo cat "$DBG_DIR/stats" > "$RECORD_DIR/debugfs_stats_before.txt" || true
    sudo cat "$DBG_DIR/queues" > "$RECORD_DIR/debugfs_queues_before.txt" || true
    sudo cat "$DBG_DIR/timeline" > "$RECORD_DIR/debugfs_timeline_before.txt" || true
fi
ip -s link show dev "$IFNAME" > "$RECORD_DIR/ip_link_before.txt" || true

sudo "$TOOLS_DIR/recv_stage09_frame" --ifname "$IFNAME" --proto "$PROTO" --timeout-sec 5 --count 1 > "$RECORD_DIR/recv.txt" 2>&1 &
RECV_PID=$!
sleep 0.5
"$TOOLS_DIR/send_stage09_frame" --ifname "$IFNAME" --proto "$PROTO" --count "$COUNT" > "$RECORD_DIR/send.txt" 2>&1 || true
wait "$RECV_PID" || true

if [[ -d "$DBG_DIR" ]]; then
    sudo cat "$DBG_DIR/stats" > "$RECORD_DIR/debugfs_stats_after.txt" || true
    sudo cat "$DBG_DIR/queues" > "$RECORD_DIR/debugfs_queues_after.txt" || true
    sudo cat "$DBG_DIR/timeline" > "$RECORD_DIR/debugfs_timeline_after.txt" || true
fi
ip -s link show dev "$IFNAME" > "$RECORD_DIR/ip_link_after.txt" || true
dmesg | tail -n 200 > "$RECORD_DIR/dmesg_tail.txt" || true

"$ROOT_DIR/scripts/queue_dist_check.sh" "$RECORD_DIR" > "$RECORD_DIR/queue_dist_check.txt" 2>&1 || true
"$ROOT_DIR/scripts/timeline_check.sh" "$RECORD_DIR" > "$RECORD_DIR/timeline_check.txt" 2>&1 || true

cat > "$RECORD_DIR/SMOKE_REPORT.md" <<REPORT
# stage09 smoke report
- interface: $IFNAME
- proto: $PROTO
- count: $COUNT
- record_dir: $RECORD_DIR
REPORT

echo "[stage09] smoke artifacts -> $RECORD_DIR"
