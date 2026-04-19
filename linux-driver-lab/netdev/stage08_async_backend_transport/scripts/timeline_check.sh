#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# timeline_check.sh — 验证 stage08 异步时序链路的正确性
# =============================================================================
#
# 【学习要点】
#
# 1. 什么是 timeline 差分？
#    stage08 在每个关键事件发生时记录一个 ns 级别的时间戳（ktime_get_raw_ns()）。
#    timeline debugfs 文件保存最近一次完整事务的 7 个时间戳：
#      last_submit_ns        : ndo_start_xmit 被调用的时刻
#      last_doorbell_ns      : virtio queue 敲 doorbell 的时刻
#      last_backend_wakeup_ns : backend_workfn 开始执行的时刻
#      last_backend_done_ns   : backend_workfn 处理完当前批次的时刻
#      last_irq_ns            : 硬件中断发生的时刻
#      last_poll_ns           : NAPI poll() 开始执行的时刻
#      last_complete_ns       : TX completion 释放 skb 的时刻
#
#    差分时间戳（delta）揭示了每一段的实际耗时，是证明异步模型的核心证据。
#
# 2. 关键差分指标解读
#
#    delta_submit_to_doorbell_ns
#    - 定义：submit() 被调用 → doorbell 敲下
#    - 正常值：几百纳秒（~140ns）
#    - 含义：这两个操作都在同一上下文（hardirq/softirq），所以极快
#    - 如果过大（>10μs）：说明 submit 函数里有阻塞操作
#
#    delta_doorbell_to_backend_ns  【核心指标】
#    - 定义：doorbell 敲下 → backend_workfn 开始执行
#    - 正常值：微秒到毫秒级（stage08 模拟 ~13μs）
#    - 含义：**这是异步模型的核心证据**
#    - doorbell_to_backend > 0 说明 backend 是被 schedule_work() 异步触发的
#    - 如果 == 0：说明 backend 是同步调用的，没有实现真正的异步分离
#
#    delta_backend_to_irq_ns
#    - 定义：backend 处理完 → 硬件中断发生（virtio 触发）
#    - 正常值：几百纳秒（~70ns）
#    - 含义：backend 最后一步是写 virtio 寄存器触发中断，耗时极短
#
#    delta_irq_to_poll_ns
#    - 定义：硬件中断 → NAPI poll() 被调用
#    - 正常值：微秒级（~2μs）
#    - 含义：中断处理函数（top half）触发软中断，软中断调度 NAPI
#
# 3. 为什么必须 > 0 而不是 == 0？
#    doorbell_to_backend > 0 是"异步"的数学定义。
#    如果为 0，说明 backend 是在 doorbell 调用栈上直接运行的（同步调用），
#    那就失去了前后端分离的意义（和单线程轮询没有区别）。
#
# 4. timeline 文件是"最近一次"的快照
#    它不是累计历史，而是最近一次完整 TX→RX 事务的时序。
#    这意味着每次发送后（或者每次 smoke 测试后）立即读取，才能拿到有意义的数据。
#    如果延迟读取，最近一次事务可能已经被新的覆盖了。
#
# 5. ktime_get_raw_ns() vs ktime_get_ts()
#    ktime_get_raw_ns() 直接读取硬件计数器，不受时间调整影响（nodelta-adjust）。
#    在性能测试中用它来计量时间差，避免 adjtimex 等时间调整引入的误差。
#
# 6. ns 精度在用户空间
#    bash 不支持真正的 64 位整数运算（>2^63 会溢出）。
#    但 delta_submit_to_doorbell_ns 等值通常在 10^9（1秒）以内，用 bash 整数够用。
#
# =============================================================================

set -euo pipefail

# =============================================================================
# get_value — 从 timeline 文件中提取 key=value
# =============================================================================
# 与 stats_check.sh 共用相同的解析逻辑（见 stats_check.sh 注释）
get_value() {
    local file=$1 key=$2
    awk -F= -v k="$key" '$1==k {print $2; found=1} END { if (!found) print "" }' "$file"
}

# =============================================================================
# 无参数模式：直接读取当前 timeline
# =============================================================================
if [[ $# -eq 0 ]]; then
    DBG_DIR=/sys/kernel/debug/netdev_stage08
    test -d "$DBG_DIR" || { echo "[stage08] debugfs dir not found: $DBG_DIR" >&2; exit 1; }
    test -f "$DBG_DIR/timeline" || { echo "[stage08] missing $DBG_DIR/timeline" >&2; exit 1; }
    sudo cat "$DBG_DIR/timeline"
    exit 0
fi

# =============================================================================
# 主验证逻辑
# =============================================================================
LOG_DIR=$1
AFTER="$LOG_DIR/debugfs_timeline_after.txt"

# 【学习】before 文件不需要，因为 timeline 是"最近一次"快照
# 只需要 after，因为 after 记录的就是"我们发送的帧"的时序
[[ -f "$AFTER" ]] || { echo "[stage08] missing timeline after file under $LOG_DIR" >&2; exit 2; }

# =============================================================================
# 提取各段 delta
# =============================================================================
submit_to_doorbell=$(get_value "$AFTER" delta_submit_to_doorbell_ns)
doorbell_to_backend=$(get_value "$AFTER" delta_doorbell_to_backend_ns)
backend_to_irq=$(get_value "$AFTER" delta_backend_to_irq_ns)
irq_to_poll=$(get_value "$AFTER" delta_irq_to_poll_ns)

# =============================================================================
# 合法性检查：每个 delta 都必须存在（非空）
# =============================================================================
# 如果 timeline 文件格式不对或字段缺失，这里会捕获
[[ -n "$submit_to_doorbell" ]] || { echo "[stage08] missing delta_submit_to_doorbell_ns" >&2; exit 1; }
[[ -n "$doorbell_to_backend" ]] || { echo "[stage08] missing delta_doorbell_to_backend_ns" >&2; exit 1; }
[[ -n "$backend_to_irq" ]] || { echo "[stage08] missing delta_backend_to_irq_ns" >&2; exit 1; }
[[ -n "$irq_to_poll" ]] || { echo "[stage08] missing delta_irq_to_poll_ns" >&2; exit 1; }

# =============================================================================
# 硬性断言：异步链必须成立
# =============================================================================
# 【学习】这里的断言是"必须通过，否则代码有根本性错误"
#
# submit→doorbell: 必须 >= 0（时间不可能倒退，但允许极小值如 0）
# doorbell→backend: 必须 > 0（核心断言，证明异步）
# backend→irq: 必须 >= 0（允许 irq 和 backend 几乎同时发生）
# irq→poll: 必须 >= 0（允许 poll 立即被调用）

(( submit_to_doorbell >= 0 )) || { echo "[stage08] submit->doorbell delta < 0" >&2; exit 1; }
(( doorbell_to_backend > 0 )) || { echo "[stage08] doorbell->backend delta must be > 0 to prove async" >&2; exit 1; }
(( backend_to_irq >= 0 )) || { echo "[stage08] backend->irq delta < 0" >&2; exit 1; }
(( irq_to_poll >= 0 )) || { echo "[stage08] irq->poll delta < 0" >&2; exit 1; }

# =============================================================================
# 输出结果
# =============================================================================
echo "timeline check passed:"
echo "submit->doorbell = ${submit_to_doorbell}ns"
echo "doorbell->backend = ${doorbell_to_backend}ns"
echo "backend->irq = ${backend_to_irq}ns"
echo "irq->poll = ${irq_to_poll}ns"