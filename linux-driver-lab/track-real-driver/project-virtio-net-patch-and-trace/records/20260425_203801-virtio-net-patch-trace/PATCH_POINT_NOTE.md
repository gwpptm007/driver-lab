# PATCH POINT NOTE

## 准备改哪里
在 `virtnet_poll()` 函数中，每次 poll 被调用时对 `poll_count` 原子变量 +1。

具体改动点：
- `struct virtnet_rq_stats` 新增 `u64 poll_count`
- `virtnet_sq_stats_desc` 新增 `poll_count` 描述符
- `virtnet_poll()` 入口处 `atomic64_inc(&rq->stats.poll_count)`

## 为什么选这里
1. 和 `lab-virtio-net-queue-poll-observe` 直接对应 — 观测 napi_poll 调用次数
2. 风险极低 — 只增加统计，不改变任何数据路径逻辑
3. 验证容易 — before/after 差值为 1 表示 poll 被调用过
4. 源码位置清晰 — `virtnet_poll` 在 line 1525

## 预期 before/after 会看到什么
- Before: `ethtool -S` 无 `poll_count` 字段
- After: `ethtool -S` 显示 `rx_queue_0_poll_count` 等字段
- Ping 前后 poll_count 差值 > 0 证明 poll 确实被调用

## 和前面几个 Lab 的关系
- `lab-virtio-net-queue-poll-observe` → 观察到 napi_poll 事件链
- `lab-virtio-net-source-dive` → 确认 virtnet_poll 源码位置和签名
- 本 patch → 把"观测到的 poll 调用"变成可计数的 stat
EOF" 2>&1
