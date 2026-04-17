# STAGE_OVERVIEW

## stage07 核心目标

> 在保持教学可解释性的前提下，把当前自研 netdev 的 ring / DMA / NAPI 模型推进到更接近真实队列驱动的组织方式。

**一句话边界**：stage07 是"真实驱动建模"的第一跳，不是"功能大杂烩"的起点。

### 本阶段要做
- 单队列 queue model
- index 驱动的 queue lifecycle
- notify / irq / napi / completion 边界
- 与 `virtio-net` 的结构映射
- stats / trace / dump 体系

### 本阶段先不做
多队列、RSS / RPS / XPS、GRO/GSO/TSO/UFO、XDP、物理硬件适配、极限性能调优

---

## 队列模型与数据结构

### 核心数据结构

```c
struct stage07_desc {
    dma_addr_t dma;
    u32 len;
    u16 flags;
    u16 cookie;
};

struct stage07_buf_slot {
    struct sk_buff *skb;
    dma_addr_t dma;
    u32 buf_len;
    u16 state;   // FREE / POSTED / SUBMITTED / DONE
    u16 id;
};

struct stage07_queue {
    struct stage07_desc *desc;
    struct stage07_buf_slot *slots;
    u16 size;
    u16 submit_idx;   // TX: 软件提交位置
    u16 notify_idx;   // TX: backend 消费位置
    u16 complete_idx; // TX: 软件回收位置
    u16 post_idx;     // RX: 软件补充位置
    u16 device_idx;   // RX: backend 写入位置
    u16 consume_idx;  // RX: 软件消费位置
    spinlock_t lock;
};
```

### 四类核心 index

| 方向 | index 对 | 含义 |
|------|---------|------|
| TX | `submit_idx` | 软件提交下一个 TX 描述符的位置 |
| TX | `notify_idx` | backend 消费位置 |
| TX | `complete_idx` | 软件回收已完成 TX 的位置 |
| RX | `post_idx` | 软件补充空 RX buffer 的位置 |
| RX | `device_idx` | backend 写入位置 |
| RX | `consume_idx` | 软件消费已完成 RX 包的位置 |

### slot 状态机

```
FREE → POSTED (RX) → SUBMITTED (TX) → DONE → FREE
```

### 设计原则

1. index 推进必须单义
2. 一个 helper 只做一件事
3. RX consume 与 RX refill 不混在一起
4. TX submit 与 TX complete 不混在一起
5. 所有状态变化都可统计、可打印、可验证

---

## NAPI / IRQ / Completion 边界

### 路径分解

**TX submit path**：
1. netdev xmit 收到 skb
2. 分配/检查 TX queue slot
3. map DMA / 填 desc
4. 推进 `submit_idx`
5. 调用 `stage07_kick_device()`（同步 notify）

**device progress path**：
1. backend 模型推进 queue
2. 标记已完成 descriptor
3. 触发 irq 或 schedule napi

**IRQ path**（只做触发，不做复杂包处理）：
- ack/计数
- 关闭中断或抑制重复触发
- `napi_schedule()`

**NAPI poll path**：
- TX completion 回收
- RX consume 上送
- RX refill
- budget 判断
- 条件满足时 `napi_complete_done()` 并 re-enable irq

### 一句话原则

> **irq 只负责触发，poll 负责批处理，queue helper 负责状态推进。**

---

## 与 virtio-net 的结构映射

### TX 路径映射

| stage07 概念 | virtio-net 对应 | 说明 |
|-------------|----------------|------|
| `submit_idx` | avail ring `idx` | CPU 提交 descriptor 到 avail ring |
| `notify_idx` | device 消费 avail ring | device 读取 avail->ring[head] |
| `complete_idx` | used ring `idx` | device 写回 used ring |
| `stage07_kick_device()` | `virtnet_tx()` + kick | 通知 backend 处理 |

### RX 路径映射

| stage07 概念 | virtio-net 对应 | 说明 |
|-------------|----------------|------|
| `post_idx` | avail ring 预先放入 buffer | 提前 post RX buffer |
| `device_idx` | device 写入 RX buffer | device DMA 填充数据 |
| `consume_idx` | used ring 读出已填充 buffer | CPU 取回完成的 RX |
| `stage07_refill_one()` | 重新补充 avail ring | 保证 RX slot 不空 |

### 关键映射思想

> 把 virtio-net 的核心组织思想，翻译成了你自己可控、可解释的模型。

本阶段不追求：feature negotiation、mergeable buffer、multiqueue、XDP hooks、control virtqueue。先把 queue lifecycle 映射清楚更重要。
