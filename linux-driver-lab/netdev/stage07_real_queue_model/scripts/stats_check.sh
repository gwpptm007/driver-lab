#!/usr/bin/env bash
# ================================================================================
# stage07 stats_check.sh — 读取 debugfs 统计信息
#
# 【功能】
# 读取 stage07 的 debugfs 统计和 queue dump，用于验证数据路径是否正常。
#
# 【输出】
# - /sys/kernel/debug/netdev_stage07/stats  : 所有 TX/RX/NAPI/device 计数器
# - /sys/kernel/debug/netdev_stage07/queues : TX/RX queue index 和 slot 状态
#
# 【用途】
# - smoke 测试后查看统计
# - 手动验证数据路径
# - 调试丢包或计数异常
#
# 【需要 root 权限】
# debugfs 默认需要 sudo 访问
# ================================================================================

set -euo pipefail

# debugfs 路径（模块加载后自动创建）
DBG_DIR=/sys/kernel/debug/netdev_stage07

# 检查 debugfs 目录是否存在（模块未加载时不存在）
test -d "$DBG_DIR" || { echo "[stage07] debugfs dir not found: $DBG_DIR" >&2; exit 1; }

# 读取所有统计计数器
# - TX/RX packets/bytes/dropped
# - TX submit/complete count
# - RX post/consume/refill count
# - NAPI poll/complete count
# - device notify/processed count
sudo cat "$DBG_DIR/stats"
echo

# 读取 TX/RX queue dump
# - TX: submit/notify/complete index 和各 slot state
# - RX: post/device/consume index 和各 slot state
sudo cat "$DBG_DIR/queues"
