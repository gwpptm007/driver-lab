#!/usr/bin/env bash
# ================================================================================
# stage07 trace_smoke.sh — 抓取 stage07 相关的 dmesg 日志
#
# 【功能】
# 从 kernel dmesg 中提取 stage07 相关日志，用于追踪数据路径行为。
#
# 【使用场景】
# - smoke 测试后查看 dmesg 中是否有异常错误
# - 对比发送前后 dmesg 变化
# - 分析 pr_info / pr_err 输出
#
# 【用法】
#   ./scripts/trace_smoke.sh                    # 输出到屏幕 + 默认文件
#   ./scripts/trace_smoke.sh /tmp/my_trace.log  # 输出到指定文件
#
# 【grep 过滤内容】
# - stage07      : 模块加载/卸载日志
# - netdev_stage07: 模块前缀（调试信息）
#
# 【输出格式】
# dmesg 是环形缓冲区，只保留最近的日志。
# tail -n 200 限制只取最近 200 行。
# ================================================================================

set -euo pipefail

# STAMP: 时间戳，用于生成唯一文件名
STAMP=$(date +%Y%m%d-%H%M%S)

# OUT: 输出文件（默认 stage07_dmesg_<STAMP>.log）
# $1: 可选，指定输出文件名
OUT=${1:-stage07_dmesg_${STAMP}.log}

# 从 dmesg 提取 stage07 相关行
# - grep -E: 支持扩展正则（stage07|netdev_stage07 匹配两种模式）
# - tail -n 200: 只取最近 200 行（dmesg 是 ring buffer）
sudo dmesg | grep -E 'stage07|netdev_stage07' | tail -n 200 | tee "$OUT"

echo "[stage07] trace log -> $OUT"
