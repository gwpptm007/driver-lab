# 01_STAGE_OVERVIEW — 架构总览

## 一句话定位

> stage08 解决"单队列异步 transport"；stage09 解决"多队列与队列分发"。

stage09 在 stage08 的基础上，将单队列扩展为多队列，每队列拥有独立的 TX/RX ring、NAPI、backend_work、stats 和 timeline。

---

## 核心目标

在 stage08 单队列异步 backend 的基础上，引入多 queue 与分发模型，让驱动开始具备更像真实 NIC / virtio 多 queue 的组织方式。

当前 v1 聚焦：
- 2 queue 起步（可配置到 4）
- queue 分发（hash-based + round-robin fallback）
- queue 级 NAPI
- queue 级 backend batching
- queue 级 timeline

---

## 与 stage08 的本质区别

| 维度 | stage08 | stage09 |
|------|---------|---------|
| 队列数 | 1（全局） | 多（默认 2，最多 4） |
| NAPI | 1 个全局 NAPI | 每队列独立 NAPI |
| backend work | 1 个全局 work | 每队列独立 work item |
| timeline | 1 个全局 timeline | 每队列独立 timeline |
| stats | 全局计数器 | 每队列独立计数器 |
| 队列选择 | 无（单队列） | `ndo_select_queue` 回调 |
| alloc 方式 | `alloc_netdev` + `ether_setup` | `alloc_etherdev_mqs` |

---

## 多队列架构的核心数据结构

`struct stage09_priv` 管理 `queues[]` 数组，每个 `struct stage09_queue` 包含：
- `txq/rxq`：独立 TX/RX ring
- `napi`：独立 NAPI poll 函数
- `backend_work`：独立 backend work item
- `timeline/stats`：独立统计

这使得每个队列的行为完全隔离，类似真实 NIC 的多队列硬件通道。

```
                      ┌─────────────────────────────────────────────────────────────┐
                      │                    struct stage09_priv                     │
                      │  state_lock / backend_wq / debugfs / num_queues=2           │
                      └──────────┬──────────────────────────────────────┬──────────┘
                                 │                                      │
                    ┌────────────▼────────────┐        ┌────────────────▼────────────┐
                    │    struct stage09_queue │        │    struct stage09_queue    │
                    │           q0            │        │           q1                │
                    │  ┌──────────────────┐  │        │  ┌──────────────────┐       │
                    │  │  txq: submit_idx │  │        │  │  txq: submit_idx │       │
                    │  │  rxq: post_idx   │  │        │  │  rxq: post_idx   │       │
                    │  │  napi            │  │        │  │  napi            │       │
                    │  │  backend_work    │  │        │  │  backend_work    │       │
                    │  │  timeline        │  │        │  │  timeline        │       │
                    │  │  stats (30+)     │  │        │  │  stats (30+)     │       │
                    │  └──────────────────┘  │        │  └──────────────────┘       │
                    └───────────────────────┘        └────────────────────────────┘
```

---

## 关键设计决策

### 队列分发策略（两极分流）

```
ndo_select_queue:
  skb_get_hash() != 0  ──→  hash % num_queues  ──→  hash-based 分发（保序）
  skb_get_hash() == 0  ──→  rr_counter++ % n    ──→  round-robin 兜底
```

- **hash 优先**：同一 flow 的帧到同一队列（保序，CPU cache 友好）
- **round-robin 兜底**：无 hash 流量也能分散到各队列

### alloc_etherdev_mqs() vs alloc_netdev()

- `alloc_netdev()` + 手动 `ether_setup()`：单队列（stage08 方式）
- `alloc_etherdev_mqs(txqs, rxqs)`：内核自动创建指定数量的 TX/RX 队列

### doorbell_pending + backend_running 握手

- `doorbell_pending`：通知"有事要处理但还没处理完"
- `backend_running`：防止 backend 在上一个 work 还没执行完时被重复入队

### 统一 backend_wq

stage09 用一个统一 workqueue（`WQ_UNBOUND`）调度所有队列的 backend work：
- `WQ_UNBOUND`：work 不绑定特定 CPU，调度器决定在哪执行
- 每个队列的 `backend_work` 独立入队，互不影响

---

## 为什么要多队列？

1. **并行性**：多个 CPU 核可以同时处理不同队列的 TX/RX
2. **保序**：同一 flow 的帧到同一个队列，保证帧顺序
3. **负载均衡**：流量分散到多个队列，避免单队列成为瓶颈
4. **真实 NIC 模拟**：商用千兆/万兆网卡普遍支持 16-128 个队列
