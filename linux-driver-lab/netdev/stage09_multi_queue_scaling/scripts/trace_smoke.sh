#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# trace_smoke.sh — 收集 stage09 驱动的 dmesg 日志（用于快速诊断）
# =============================================================================
#
# 【学习要点】
#
# 1. dmesg 的作用
#    dmesg 打印内核环形缓冲区内容，包含了驱动在运行时打印的所有 pr_info/pr_err 等。
#    对于网络设备驱动，可以在这里看到：模块加载、接口 up/down、TX/RX 统计、错误信息等。
#
# 2. dmesg 是实时追加的
#    内核日志缓冲区是一个固定大小的环形 buffer，新的日志会覆盖旧的。
#    如果日志过多，需要在测试前清理一次 dmesg -C（清除缓冲区）或 tail -n 200 只看最新的。
#
# 3. grep -E 'stage09|netdev_stage09'
#    -E 启用扩展正则，可以同时匹配两个关键字
#    stage09 是模块名（出现在 pr_info 的前缀），netdev_stage09 是 driver name
#    两者同时匹配可以覆盖驱动中所有日志输出
#
# 4. tee 的双重输出
#    tee 把输出同时写到文件（归档）和 stdout（实时查看）
#    适用于测试脚本的日志记录
#
# 5. 为什么 tail -n 200
#    dmesg 可能输出数万行，但很多是系统启动早期的内容（与当前测试无关）
#    只看最近的 200 行能更聚焦于本次测试相关的日志
#
# =============================================================================

set -euo pipefail

STAMP=$(date +%Y%m%d-%H%M%S)
OUT=${1:-stage09_dmesg_${STAMP}.log}

sudo dmesg | grep -E 'stage09|netdev_stage09' | tail -n 200 | tee "$OUT"
echo "[stage09] trace log -> $OUT"
