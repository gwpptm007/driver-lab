#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# stats_check.sh — 验证 stage08 驱动统计数据的正确性
# =============================================================================
#
# 【学习要点】
#
# 1. 差分验证（Before/After Snapshot）的原理
#    在发送测试帧之前记录一份 stats（快照 A），发送之后再记录一份（快照 B）。
#    B - A 得到本次测试的实际增量，这个增量反映了真实发生的事件数量。
#    这种方法比直接读取"绝对值"更准确，因为可以排除历史累计的影响。
#
# 2. debugfs — 内核统计的用户空间窗口
#    debugfs 是内核提供的一种内存文件系统，挂载在 /sys/kernel/debug/。
#    驱动通过 debugfs_create_file() 注册文件节点，用户可以直接 cat 查看。
#    优点：零开销（不需要任何 ioctl 或 netlink），可直接读取内核变量。
#    路径：/sys/kernel/debug/netdev_stage08/stats
#
# 3. 理解 stage08 的各统计计数器
#
#    TX 路径（发送端）：
#    - tx_submit_count   : ndo_start_xmit 被调用的次数（每帧一次）
#    - doorbell_count    : 敲击 doorbell 的次数（通常=tx_submit，因为每帧都敲）
#    - backend_schedule_count : backend_work 被 schedule 的次数（>=1，批处理时可能 < doorbell）
#    - backend_run_count  : backend_worker 实际运行的次数（批处理，每次处理一批）
#    - backend_tx_processed : backend 处理完的 TX descriptor 数量
#    - tx_complete_count  : TX 完成数（帧被安全释放）
#    - tx_dropped         : 丢弃的 TX 帧（ring full 或 DMA 失败）
#    - tx_dma_map_ok/fail : DMA scatter-gather 映射成功/失败次数
#
#    RX 路径（接收端）：
#    - rx_post_count      : RX buffer 被 posted 到 ring 的次数
#    - rx_refill_count    : RX buffer 池 refill 的次数（不断补充空 buffer）
#    - rx_consume_count   : NAPI poll 中消费 RX 帧的次数
#    - rx_packets         : 实际交付给协议栈的帧数
#    - rx_dropped         : 丢弃的 RX 帧
#    - rx_dma_map_ok/fail : RX DMA 映射成功/失败
#
#    NAPI/中断相关：
#    - irq_count          : 物理中断发生次数
#    - irq_mask_count     : 中断被 mask（禁止）的次数
#    - irq_unmask_count   : 中断被 unmask（恢复）的次数
#    - napi_schedule_count: NAPI poll 被调度的次数
#    - napi_poll_count    : napi->poll() 实际被调用次数
#    - napi_complete_count : napi_complete() 被调用次数
#    - napi_budget_exhaust_count : poll 中 budget 用尽的次数（>0 说明需要更多轮次）
#
# 4. 为什么 end-state 必须全部为 0
#    测试结束后，驱动应该处于"空闲状态"：
#    - tx_inflight=0       : 没有 TX 帧卡在半路
#    - tx_done=0            : TX done ring 是空的
#    - rx_ready=0           : RX ready ring 是空的
#    - doorbell_pending=0   : 没有待处理的 doorbell
#    - backend_running=0    : backend worker 已经停止
#    任一非零都意味着有资源泄漏或状态机卡死。
#
# 5. test_stats 的强校验
#    test_stats 是专为测试帧设计的统计（比 stats 更精确）。
#    如果 debugfs 中有 test_stats 文件，stats_check 会用 check_eq（精确等于）
#    而不是 check_ge（大于等于），给出更强的保证。
#
# 6. awk -F= 解析 key=value 文件
#    debugfs 输出格式是每行 key=value，如 tx_submit_count=51
#    -F= 指定等号为分隔符，$1 是 key，$2 是 value
#    这种纯文本格式易于人工阅读，也易于脚本解析。
#
# =============================================================================

set -euo pipefail

# =============================================================================
# get_value — 从 key=value 文件中提取指定 key 的值
# =============================================================================
# 【学习】awk 的习惯用法
# awk -F= -v k="$key" '$1==k {print $2; found=1} END {...}'
# -F=           : 以等号分隔字段
# -v k="$key"   : 传入 shell 变量作为 awk 变量
# $1==k         : 第一列（key）精确匹配目标
# found=1        : 标记是否找到（用于 END 中判断）
# END           : 文件处理完毕后执行
# 如果没找到，found 保持为空，END 打印空字符串
get_value() {
    local file=$1 key=$2
    awk -F= -v k="$key" '$1==k {print $2; found=1} END { if (!found) print "" }' "$file"
}

# =============================================================================
# delta_of — 计算同一个 key 在 before/after 两个文件中的差值
# =============================================================================
# 【学习】差分验证的核心函数
# 分别从 before 和 after 文件读取同一个 key，相减得到增量
# 用 $((b - a)) 做算术计算（bash 原生整数运算）
# 如果任一值为空（文件缺失 key），返回 1（失败）
delta_of() {
    local before=$1 after=$2 key=$3
    local a b
    a=$(get_value "$before" "$key")
    b=$(get_value "$after" "$key")
    [[ -n "$a" && -n "$b" ]] || return 1
    echo $((b - a))
}

# =============================================================================
# 无参数模式：直接读取当前 debugfs 状态
# =============================================================================
if [[ $# -eq 0 ]]; then
    DBG_DIR=/sys/kernel/debug/netdev_stage08
    test -d "$DBG_DIR" || { echo "[stage08] debugfs dir not found: $DBG_DIR" >&2; exit 1; }

    # 直接 cat 会把整个 stats 文件内容打印到 stdout
    sudo cat "$DBG_DIR/stats"
    echo

    # test_stats 是可选的，如果存在就打印
    [[ -f "$DBG_DIR/test_stats" ]] && sudo cat "$DBG_DIR/test_stats"
    echo

    sudo cat "$DBG_DIR/queues"
    exit 0
fi

# =============================================================================
# 主验证逻辑：接收 log_dir 和预期帧数
# =============================================================================
LOG_DIR=$1
# 【学习】第二个参数是预期帧数，用于强校验（默认 32）
EXPECTED=${2:-32}
BEFORE="$LOG_DIR/debugfs_stats_before.txt"
AFTER="$LOG_DIR/debugfs_stats_after.txt"
TEST_BEFORE="$LOG_DIR/debugfs_test_stats_before.txt"
TEST_AFTER="$LOG_DIR/debugfs_test_stats_after.txt"

# 缺少 before/after 文件是致命错误，直接退出
[[ -f "$BEFORE" && -f "$AFTER" ]] || { echo "[stage08] missing stats before/after under $LOG_DIR" >&2; exit 2; }

# =============================================================================
# 辅助函数：校验函数
# =============================================================================
# 【学习】set -e 的安全边界
# (( value >= need )) || fail ...
# 在 value < need 时 (( )) 返回 1，但 set -e 不会导致脚本退出
# 因为 || fail 捕获了返回值，所以用 || 组合是安全的

fail() {
    echo "[stage08] stats check failed: $*" >&2
    exit 1
}

# check_ge: Greater Than or Equal，用于 TX/RX 等计数（允许多余，不允许不足）
# 【学习】为什么要用 >= 而不是 ==？
# 因为 debugfs 快照之间可能有其他后台任务也使用了驱动（不是我们的测试帧）
# 所以计数"只多不少"是合理的期望
check_ge() {
    local name=$1 value=$2 need=$3
    echo "$name delta=$value (need >= $need)"
    (( value >= need )) || fail "$name delta=$value < $need"
}

# check_eq: Equal，用于 test_stats（精确计数，每帧都有记录）
check_eq() {
    local name=$1 value=$2 need=$3
    echo "$name delta=$value (need == $need)"
    (( value == need )) || fail "$name delta=$value != $need"
}

# check_zero_after: 用于 end-state 检查，值必须为 0
check_zero_after() {
    local key=$1 value
    value=$(get_value "$AFTER" "$key")
    [[ -n "$value" ]] || fail "missing after key $key"
    echo "$key after=$value (need == 0)"
    (( value == 0 )) || fail "$key after=$value != 0"
}

# =============================================================================
# 差分计算
# =============================================================================
# 【学习】这些 delta 反映了"从 before 到 after 发生了什么"
tx_submit_delta=$(delta_of "$BEFORE" "$AFTER" tx_submit_count)
tx_complete_delta=$(delta_of "$BEFORE" "$AFTER" tx_complete_count)
doorbell_delta=$(delta_of "$BEFORE" "$AFTER" doorbell_count)
backend_sched_delta=$(delta_of "$BEFORE" "$AFTER" backend_schedule_count)
backend_run_delta=$(delta_of "$BEFORE" "$AFTER" backend_run_count)
backend_tx_delta=$(delta_of "$BEFORE" "$AFTER" backend_tx_processed)
backend_rx_delta=$(delta_of "$BEFORE" "$AFTER" backend_rx_produced)
rx_consume_delta=$(delta_of "$BEFORE" "$AFTER" rx_consume_count)
irq_delta=$(delta_of "$BEFORE" "$AFTER" irq_count)
napi_poll_delta=$(delta_of "$BEFORE" "$AFTER" napi_poll_count)

# =============================================================================
# 校验 TX 路径
# =============================================================================
# 【学习】TX 路径的校验逻辑
# 每帧都会触发：submit → doorbell → backend_schedule → backend_run
# 如果这些 delta 都不小于预期帧数，说明链路完整
check_ge tx_submit_count "$tx_submit_delta" "$EXPECTED"
check_ge tx_complete_count "$tx_complete_delta" "$EXPECTED"
# doorbell >= 1 是最低要求（至少敲了一次）
check_ge doorbell_count "$doorbell_delta" 1
check_ge backend_schedule_count "$backend_sched_delta" 1
check_ge backend_run_count "$backend_run_delta" 1
check_ge backend_tx_processed "$backend_tx_delta" "$EXPECTED"

# =============================================================================
# 校验 RX 路径
# =============================================================================
# 【学习】RX 路径：backend 产生帧 → irq → poll → consume → refill
# backend_rx_produced 记录了 backend 向 RX ring 写入的帧数
# rx_consume_count 记录了 NAPI poll 消费了多少帧
# 两者都应该 >= EXPECTED
check_ge backend_rx_produced "$backend_rx_delta" "$EXPECTED"
check_ge rx_consume_count "$rx_consume_delta" "$EXPECTED"

# =============================================================================
# 校验 NAPI/中断
# =============================================================================
# 【学习】irq 和 poll 的关系
# 在 stage08 中，backend 每批处理完会 raise 一个 irq
# irq_count >= 1 说明中断机制工作
# napi_poll_count >= 1 说明 NAPI 被调度
check_ge irq_count "$irq_delta" 1
check_ge napi_poll_count "$napi_poll_delta" 1

# =============================================================================
# 强校验：如果 test_stats 存在，用精确等于
# =============================================================================
# 【学习】为什么有了 check_ge 还需要 check_eq？
# 因为 test_stats 是专门记录"测试帧"的计数器
# 它的值和 EXPECTED 完全相等，证明没有其他帧干扰测试
if [[ -f "$TEST_BEFORE" && -f "$TEST_AFTER" ]]; then
    test_tx_submit_delta=$(delta_of "$TEST_BEFORE" "$TEST_AFTER" test_tx_submit_count)
    test_backend_tx_delta=$(delta_of "$TEST_BEFORE" "$TEST_AFTER" test_backend_tx_processed)
    test_backend_rx_delta=$(delta_of "$TEST_BEFORE" "$TEST_AFTER" test_backend_rx_produced)
    test_rx_consume_delta=$(delta_of "$TEST_BEFORE" "$TEST_AFTER" test_rx_consume_count)
    echo
    echo "[stage08] test_stats present, using strong checks"
    check_eq test_tx_submit_count "$test_tx_submit_delta" "$EXPECTED"
    check_eq test_backend_tx_processed "$test_backend_tx_delta" "$EXPECTED"
    check_eq test_backend_rx_produced "$test_backend_rx_delta" "$EXPECTED"
    check_eq test_rx_consume_count "$test_rx_consume_delta" "$EXPECTED"
fi

# =============================================================================
# end-state 清洁检查
# =============================================================================
# 【学习】end-state 为 0 是"完全清理"的证据
# 任何非零值都意味着有资源没有正确释放
echo
check_zero_after tx_inflight
check_zero_after tx_done
check_zero_after rx_ready
check_zero_after doorbell_pending
check_zero_after backend_running

echo "[stage08] stats check passed"