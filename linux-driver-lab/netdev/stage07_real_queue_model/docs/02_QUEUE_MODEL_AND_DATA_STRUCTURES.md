# 02_QUEUE_MODEL_AND_DATA_STRUCTURES

## 设计目标

当前 stage04 的重点是 descriptor owner / DMA / refill。

stage07 要在此基础上继续明确：

- 谁提交（producer）
- 谁完成（consumer）
- 哪些 index 推进
- 哪些状态由 CPU 持有
- 哪些状态表示 device 完成

## 建议数据结构

### 1. descriptor

建议把 descriptor 与 slot 元数据明确拆出来。

```c
struct stage07_desc {
    dma_addr_t dma;
    u32 len;
    u16 flags;
    u16 cookie;
};
```

### 2. buffer slot

```c
struct stage07_buf_slot {
    struct sk_buff *skb;
    dma_addr_t dma;
    u32 buf_len;
    u16 state;
    u16 id;
};
```

### 3. queue

```c
struct stage07_queue {
    struct stage07_desc *desc;
    struct stage07_buf_slot *slots;
    u16 size;
    u16 submit_idx;
    u16 complete_idx;
    u16 post_idx;
    u16 consume_idx;
    u16 pending;
    spinlock_t lock;
};
```

## 四类核心 index

### TX
- `submit_idx`：软件提交下一个 TX 描述符的位置
- `complete_idx`：软件回收已完成 TX 的位置

### RX
- `post_idx`：软件补充空 RX buffer 的位置
- `consume_idx`：软件消费已完成 RX 包的位置

## 为什么要这样拆

因为真实驱动理解的关键，不只是“ring 上有 desc”，而是：

- 谁推进 head/tail
- 谁负责通知
- 谁负责回收
- queue 满/空怎么判断
- 生命周期是否可观测

## slot state 建议

建议至少定义：

- `S07_SLOT_FREE`
- `S07_SLOT_POSTED`
- `S07_SLOT_INFLIGHT`
- `S07_SLOT_DONE`
- `S07_SLOT_CONSUMED`

## queue helper 建议

建议把以下 helper 明确成独立函数：

- `stage07_queue_is_full()`
- `stage07_queue_is_empty()`
- `stage07_tx_submit_one()`
- `stage07_tx_complete_one()`
- `stage07_rx_post_one()`
- `stage07_rx_consume_one()`
- `stage07_rx_refill_one()`

## 设计原则

1. index 推进必须单义
2. 一个 helper 只做一件事
3. RX consume 与 RX refill 不混在一起
4. TX submit 与 TX complete 不混在一起
5. 所有状态变化都可统计、可打印、可验证
