# 02_QUEUE_MODEL — 每队列数据结构与 ring 模型

## 每个队列的独立执行上下文

stage09 的核心设计：每个队列都有完整的 Front-end → Back-end → NAPI 数据通路，这模拟了真实 NIC 多队列的独立硬件通道。

```
┌─────────────────────────────────────────────────────────┐
│                    struct stage09_queue                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐  │
│  │   txq    │  │   rxq    │  │   napi   │  │ backend │  │
│  │ (ring)   │  │ (ring)   │  │          │  │  _work  │  │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘  │
│  ┌──────────────┐  ┌──────────────────────────────┐     │
│  │  timeline    │  │       stats                  │     │
│  │  (8 ts)     │  │  (30+ atomic counters)       │     │
│  └──────────────┘  └──────────────────────────────┘     │
│  doorbell_pending | backend_running | irq_masked       │
└─────────────────────────────────────────────────────────┘
```

---

## TX Ring 的 6 个 index（生产者/消费者模型）

| index | 含义 | 掌控者 |
|--------|------|--------|
| `submit_idx` | 下一个可用的 TX slot | ndo_start_xmit（生产者） |
| `notify_idx` | backend 已通知处理到哪个 slot | backend_workfn |
| `complete_idx` | NAPI 已回收到哪个 slot | napi_poll |
| `inflight` | submit_idx - complete_idx（飞行中帧数） | 派生值 |

两个生产者（ndo_start_xmit 写 submit_idx，backend 写 notify_idx）和一个消费者（napi_poll 读/写 complete_idx）。

---

## RX Ring 的 6 个 index

| index | 含义 | 掌控者 |
|--------|------|--------|
| `post_idx` | 下一个填充 RX buffer 的位置 | napi_poll / stage09_refill_rx_all |
| `device_idx` | backend 生产到哪个 slot | backend_workfn（生产者） |
| `consume_idx` | NAPI 已消费到哪个 slot | napi_poll |
| `posted` | 已填充未处理的 RX buffer 数 | 派生值 |

backend 是 RX 生产者，napi_poll 是 RX 消费者。

---

## slot 状态机

**TX slot 状态机**：`FREE → SUBMITTED → DONE → FREE`

- `FREE`：可用slot
- `SUBMITTED`：帧已提交，等待 backend 处理
- `DONE`：backend 处理完，等待 NAPI 回收

**RX slot 状态机**：`FREE → POSTED → DONE → FREE`

- `FREE`：可填充
- `POSTED`：buffer 已填充，等待 backend 生产
- `DONE`：数据就绪，等待 NAPI 消费

两种 ring 共享同一状态枚举，因为语义相同。

---

## struct stage09_desc — DMA 环描述符

```c
struct stage09_desc {
    dma_addr_t dma_addr;  /* 数据的 DMA 总线地址 */
    u32 data_len;          /* 数据长度（字节） */
    u16 state;             /* 当前状态（enum stage09_slot_state） */
    u16 flags;             /* 保留，备用 */
};
```

descriptor 是 DMA 环的描述符，直接写入硬件。与 `buf_slot` 不同，desc 只包含 DMA 需要的最小信息。真实 virtio-net 的 `vring_desc` 就是这个结构。

---

## struct stage09_buf_slot — skb 容器

```c
struct stage09_buf_slot {
    struct sk_buff *skb;   /* 关联的 socket buffer */
    dma_addr_t dma_addr;   /* skb->data 的 DMA 地址 */
    u32 buf_len;           /* buffer 总长度（skb->truesize） */
    u32 data_len;          /* 实际数据长度 */
    u16 state;             /* slot 状态（必须与 desc->state 一致） */
    u16 id;                /* slot 索引，用于调试 */
    u32 last_seq;          /* 最近一次 test 帧的 sequence number */
};
```

`state` 字段与 `desc->state` 同步（两处都要改，保持一致）。

---

## struct stage09_timeline — 每队列独立时间戳链

每个队列记录最近一次完整 TX→RX 事务的 8 个时间戳：

```
last_submit_ns → last_doorbell_ns → last_backend_wakeup_ns → last_backend_done_ns
    → last_irq_ns → last_poll_ns → last_complete_ns → last_consume_ns
```

关键 4 个 delta 解读：

| delta | 含义 | 期望值 |
|--------|------|--------|
| `submit_to_doorbell` | submit 到 doorbell（同一上下文） | ~140ns |
| `doorbell_to_backend` | doorbell 到 backend 执行（**异步核心**） | **>0 证明异步** |
| `backend_to_irq` | backend 处理完到 irq | ~70ns |
| `irq_to_poll` | irq 到 NAPI poll | ~2μs |

`doorbell_to_backend_ns > 0` 是证明"异步"的数学定义：如果 == 0，说明 backend 是在 doorbell 调用栈上直接运行（同步）。

---

## struct stage09_queue_stats — 30+ 原子计数器

**TX 路径统计**（发送端）：
- `tx_submit_count`：提交到 ring 的 TX 帧数
- `tx_complete_count`：NAPI 回收的 TX 帧数
- `tx_packets / tx_bytes`：成功映射的 TX 总数
- `tx_busy`：因 ring full 返回 `NETDEV_TX_BUSY` 的次数
- `tx_dropped`：丢弃的 TX 帧数
- `tx_linearize_count`：skb linearize 失败次数
- `tx_dma_map_ok/fail`：DMA 映射成功/失败

**RX 路径统计**（接收端）：
- `rx_post_count`：RX buffer 被 posted 到 ring 的次数
- `rx_consume_count`：NAPI poll 消费的 RX 帧数
- `rx_packets / rx_bytes`：交付给协议栈的总计
- `rx_dropped`：丢弃的 RX 帧数

**Backend 路径统计**：
- `doorbell_count`：doorbell 敲击次数
- `backend_schedule_count`：backend work 入队次数
- `backend_run_count`：backend_workfn 实际执行的次数
- `backend_tx_processed / backend_rx_produced`：处理的 descriptor 数

**NAPI 统计**：
- `irq_count`：物理中断发生次数
- `napi_poll_count`：napi->poll() 被调用的次数
- `napi_complete_count`：napi_complete_done() 被调用的次数
- `napi_work_total`：NAPI poll 总共处理的工作量

**测试专用统计**：
- `test_tx_submit_count / test_rx_consume_count`：精确计数本次测试帧，排除历史干扰

---

## doorbell_pending + backend_running 握手机制

```
ndo_start_xmit                backend_workfn                  napi_poll
     │                              │                              │
     │── doorbell_pending=true ───→│                              │
     │── queue_work() ─────────────→│                              │
     │                              │                              │
     │                   backend_running=true                    │
     │                   处理 TX/RX...                            │
     │                   backend_running=false                   │
     │                   doorbell_pending=false                  │
     │                   (如果还有未处理，doorbell_pending=true)   │
     │                              │                              │
     │                              │── raise_irq() ─────────────→│
     │                              │                              │
```

- `doorbell_pending`：标记"有事要处理，但还没处理完"，防止 backend 还没处理完时新 work 又入队导致重复处理
- `backend_running`：防止 backend 在上一个 work 还没执行完时被重复入队
