# 02_QUEUE_MODEL — page_pool 与 RX 生命周期

## 核心设计：每队列独立 page_pool

stage11 采用**每队列独立 page_pool**（与真实驱动如 virtio-net、igb 一致）：

```c
struct stage11_queue {
    ...
    struct page_pool *pp;  // 每个队列独立
};

struct page_pool_params params = {
    .pool_size = ring_size * 2,
    .netdev = ndev,
    .queue_idx = qid,
    .napi = &q->napi,
};
```

**为什么独立 pool**：
- NUMA local 分配：每个队列的 page 在对应 NUMA node 上
- 故障隔离：一个队列 pool 满了不影响其他队列
- 资源可预期：每个队列精确控制 pool_size

---

## RX slot 结构

```c
struct stage11_buf_slot {
    struct page *page;     // 来自 page_pool 的 page
    void *buf;             // page_address(page)
    u16 buf_len;
    u16 data_len;
    enum stage11_slot_state state;
    u16 id;
    u32 last_seq;
};
```

注意：slot **不再存 skb**，而是存 page。skb 在 consume 时通过 `build_skb()` 从 page 动态构建。

---

## 5 状态 RX slot 状态机

```
TX path:  FREE → SUBMITTED → DONE → FREE

RX path:  FREE → POSTED → READY → DONE → FREE
              ↑                          │
              └───── refill ────────────┘
```

| 状态 | 含义 |
|------|------|
| `S11_SLOT_FREE` | slot 空闲，无 page |
| `S11_SLOT_POSTED` | page 已分配（来自 page_pool），等待 backend 填充 |
| `S11_SLOT_READY` | backend 已将 TX 数据复制到 page，数据就绪 |
| `S11_SLOT_DONE` | napi 已消费（或正在消费） |

**backend_workfn** 将 POSTED → READY
**napi_poll** 将 READY → DONE，并在同一 slot 上立即 refill → POSTED

---

## build_skb() 零拷贝路径

Linux kernel 提供两个从 page 构建 skb 的函数：

| 函数 | 特点 | 适用场景 |
|------|------|----------|
| `build_skb()` | 不额外 get_page，destructor 只 put_page 一次 | page_pool 场景（page 已在 pool 管理下） |
| `napi_build_skb()` | 额外 get_page，destructor put_page 两次 | 独立分配的 page |

**stage11 使用 `build_skb()`**，因为 page 来自 page_pool。

### build_skb 内部（简化）

```c
struct sk_buff *build_skb(void *data, unsigned int frag_size)
{
    struct sk_buff *skb = alloc_skb(frag_size + NET_SKB_PAD, GFP_ATOMIC);
    skb->data = data;
    skb->tail = data;
    skb->end = data + frag_size;
    skb->destructor = &skb_release_data;  // 关键！
    skb_reserve(skb, NET_SKB_PAD);
    return skb;
}
```

**关键**：`build_skb` **不调用 `get_page`**！

- page 初始 refcount = 1（来自 `page_pool_alloc_pages`）
- `build_skb` 后：refcount = 1（不增加）
- skb destructor 后：refcount = 0，page 被 page_pool 回收

**这与 page_pool 的回收机制完美配合**：无需显式 recycle，skb destructor 自动完成。

---

## RX consume 完整路径

```c
static int stage11_consume_rx_one(struct stage11_queue *q)
{
    struct page *page = s->page;
    void *buf = s->buf;
    u32 len = s->data_len;

    // 1. 从 page 构建 skb（零拷贝，无数据复制）
    skb = build_skb(buf, rx_buf_size);
    if (!skb) {
        // build_skb 失败：显式回收 page 到 page_pool
        page_pool_recycle_direct(q->pp, page);
        atomic64_inc(&q->stats.pp_build_skb_fail);
        goto recycle_slot;
    }

    // 2. 上送协议栈
    skb_put(skb, len);
    skb->protocol = eth_type_trans(skb, ndev);
    netif_receive_skb(skb);
    // 3. build_skb 成功：skb destructor 的 put_page 自动归 page 回 pool
    //    无需显式 recycle！

recycle_slot:
    // 4. 清理 slot
    memset(s, 0, sizeof(*s));
    r->consume_idx = stage11_next_idx(r->consume_idx, r->size);
    q->rx_ready--;

    // 5. 立即 refill，保持 posted 水位
    stage11_refill_rx_slot(q, idx);
    return 1;
}
```

### 成功路径（正常）

```
Page_pool_dev_alloc_pages() → page (refcount=1)
    ↓
build_skb(page) → skb (refcount 仍为 1)
    ↓
netif_receive_skb(skb)
    ↓
(skb destructor: put_page) → refcount=0 → page 返回 pool
    ↓
stage11_refill_rx_slot() → 新 page 填充 slot
```

### 失败路径（build_skb 失败）

```
build_skb() → NULL
    ↓
page_pool_recycle_direct(q->pp, page) → 显式归还 page 到 pool
    ↓
stage11_refill_rx_slot() → 新 page 填充 slot
```

---

## TX 侧（bounce buffer，不变）

soft 版本 TX 使用 bounce buffer（无 DMA）：

```c
// start_xmit: 分配 bounce buffer，复制 skb 数据
buf = kmalloc(skb_headlen(skb), GFP_ATOMIC);
memcpy(buf, skb->data, skb_headlen(skb));

// complete_tx_one: 释放 bounce buffer
kfree(s->buf);
```

真实驱动中 TX 侧会使用 DMA 映射 buffer（`dma_map_single`），page_pool 也可以管理 TX 描述符的 DMA buffer。

---

## page_pool 与 DMA sync

`PP_FLAG_DMA_SYNC_DEV` 标志要求驱动在将 page 提供给硬件之前同步：

```c
struct page_pool_params params = {
    .flags = PP_FLAG_DMA_SYNC_DEV,  // 需要 DMA sync
    ...
};
```

soft 版本不需要真实 DMA 同步，但这个标志说明了为什么高性能驱动需要 page_pool——它统一管理了 DMA coherent buffer 的生命周期。
