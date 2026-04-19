#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# timeline_check.sh — 验证 stage09 per-queue timeline 异步链路的正确性
# =============================================================================
#
# 【学习要点】
#
# 1. stage09 的 per-queue timeline
#    stage09 每个队列都有独立的 timeline
#    timeline 文件每行列式：q0: ... doorbell_to_backend_ns=13847 ...
#    需要从每行列中提取 doorbell_to_backend_ns 值
#
# 2. awk 解析多行 per-queue 格式
#    /^q[0-9]+:/ 匹配每个队列的行
#    遍历每列（NF=字段数）找到 doorbell_to_backend_ns=开头的字段
#    split($i, a, "=") 按等号分割，取第二部分为值
#    (a[2] + 0) > 0 判断值是否大于 0
#
# 3. 异步链断言
#    doorbell_to_backend_ns > 0 是"异步"的数学定义
#    如果 == 0，说明 backend 是在 doorbell 调用栈上直接运行（同步）
#    多队列下，只要有一个队列满足 > 0，就证明异步模型成立
#
# 4. 多队列 vs stage08
#    stage08 只有单个全局 timeline
#    stage09 需要从每队列的行中分别提取
#    grep -E '^q[0-9]+: ...' 能匹配所有队列行
#
# 5. awk END 块
#    文件处理完毕后执行，输出通过/失败结果
#    ok=1 表示找到了至少一个有效的 doorbell_to_backend_ns > 0
#
# =============================================================================
