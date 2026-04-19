# 03_FLOW_DISTRIBUTION_AND_NAPI — 流量分发与 per-queue NAPI

## stage09 的关键不是"数组多了"

stage09 的关键不是"把单队列变成数组"，而是：
- 每 queue 独立 poll
- 每 queue 独立 backend 执行
- 每 queue 独立统计
- **能观察分发是否只落一个 queue**

---

## 队列选择：stage09_select_queue()

内核在发送前会调用 `ndo_select_queue` 回调决定用哪个队列。

**分发策略（两极分流）**：

1. **有 hash 时**（优先）：`skb_get_hash() % num_queues`
   - hash 通常来自 5-tuple（src_ip, dst_ip, src_port, dst_port, protocol）
   - 保证同一 flow 的帧到同一个队列（**保序**）
   - `reciprocal_scale` 是比 `%` 更快的除法（针对 2^n 的优化）
   - CPU cache 友好：同一 flow 的数据在同一 CPU 处理

2. **无 hash 时**（兜底）：round-robin
   - `rr_counter` 原子递增后取模
   - 保证无 hash 流量也能分散到各队列

```c
u32 hash = skb_get_hash(skb);
if (hash)
    return reciprocal_scale(hash, priv->num_queues);
return atomic64_inc_return(&priv->rr_counter) % priv->num_queues;
```

**教学亮点：为什么 hash 优先？**
- 网络流通常有多个帧，同一 flow 到同一队列保证保序
- CPU cache 友好
- 真实 NIC RSS（Receive Side Scaling）也是类似原理

---

## per-queue NAPI

### napi 结构

每个队列有独立的 `struct napi_struct`：
```c
struct stage09_queue {
    ...
    struct napi_struct napi;   /* 独立 NAPI 结构，每队列一个 */
    ...
};
```

NAPI 注册（在 `stage09_init` 中）：
```c
STAGE09_NETIF_NAPI_ADD(ndev, &q->napi, stage09_napi_poll, napi_weight);
```

### stage09_napi_poll — per-queue 轮询函数

**上下文**：softirq（软中断），持有 `priv->state_lock`

**budget 机制**：
- 内核传入 budget，限制每次 poll 最多处理多少帧
- 防止 poll 函数霸占 CPU 太长时间，保证实时性
- 返回实际处理的工作量（`work_done`）

**两层 while 循环**：
1. 先回收 TX done（`stage09_complete_tx_one`），清空 `tx_done`
2. 再消费 RX ready（`stage09_consume_rx_one`），受 budget 限制

**napi_complete_done 条件**：
- `rx_ready == 0`（RX 全部处理完）
- `tx_done == 0`（TX 全部回收完）
- 如果还有待处理 work，重新 `mark_doorbell`（防止漏处理）

### irq_masked 机制

```c
if (!q->irq_masked && napi_schedule_prep(&q->napi)) {
    q->irq_masked = true;
    __napi_schedule(&q->napi);
}
```

- `irq_masked` 在 `napi_complete_done()` 时复位
- 当 NAPI poll 还没完成时，如果 backend 又处理完一批帧，不应该再触发 irq
- 用于"批量 irq"优化：batch 处理期间只触发一次中断

---

## TX 发送路径（ndo_start_xmit）

```
应用                      内核网络栈                 ndo_start_xmit
  │                           │                           │
  │ skb                       │ skb + queue_mapping       │
  │───────────────────────────>│──────────────────────────>│
  │                           │                           │
  │                           │  qid = mapping % nqueues   │
  │                           │                           │
  │                           │  skb_linearize (if needed)│
  │                           │  DMA map                   │
  │                           │  slot[SUBMITTED]           │
  │                           │  submit_idx++              │
  │                           │  tx_inflight++            │
  │                           │                           │
  │                           │  stage09_mark_doorbell()  │
  │                           │       │                   │
  │                           │       ▼                   │
  │                           │  doorbell_pending=true    │
  │                           │  queue_work() ──────────> backend_workfn
```

返回值的含义：
- `NETDEV_TX_OK`：成功入队（不等于已发送）
- `NETDEV_TX_BUSY`：ring full，应该重试

---

## Backend 处理路径（backend_workfn）

**上下文**：workqueue 线程（可能是任意 CPU），持有 `priv->state_lock`

**双层循环设计**：
- 外层：限制 batch（`backend_batch`），避免单次处理过长
- 内层：检查 TX slot 有数据和 RX slot 有空位，才处理

**TX→RX 数据通路（loopback 教学模型）**：
```c
copy_len = min(txd->data_len, rxs->buf_len);
skb_put(rxs->skb, copy_len);
memcpy(rxs->skb->data, txs->skb->data, copy_len);
```
- `memcpy` 模拟 DMA transfer（真实 virtio-net 用 DMA scatter-gather）
- min 防止 buffer overflow

**doorbell_pending 重入逻辑**：
- 处理完当前批后发现 `txq.notify_idx != txq.submit_idx`，说明还有未处理 TX
- 置 `doorbell_pending=true`，下次 `ndo_start_xmit` 或 NAPI complete 时可能重新入队

---

## 为什么分发验证很重要？

如果 smoke test 发现只有一个队列活跃（`queue_dist_check.sh` 失败），说明：
- 分发策略有问题（hash 没有生效，回到了 round-robin 但只有 1 个 flow）
- 或者多队列没有正确初始化
- 退化为单队列，失去了 stage09 的意义
