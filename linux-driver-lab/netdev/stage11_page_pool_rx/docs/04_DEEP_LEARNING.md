# 04_DEEP_LEARNING — page_pool 核心知识点与调用链

## 1. 为什么高性能驱动需要 page_pool

### 传统方式的成本

每帧 RX 都调用 `netdev_alloc_skb()`：
1. `alloc_skb()` — 分配 struct sk_buff（~256 bytes）+ 头部空间
2. 分配数据 buffer
3. 每帧都要经历 `alloc + free` 循环

在 10Gbps 线速（14.88Mpps）下：
- 每秒 1488 万次 `alloc_skb` / `kfree_skb`
- 每次 256~512 bytes 的分配/释放开销
- 内存碎片化

### page_pool 的解决思路

核心观察：**RX buffer 的内容（page）比容器（skb）更值得复用**。

```
传统方式：         page_pool + build_skb:
alloc_skb()        page_pool_dev_alloc_pages()
copy data          build_skb(page)  ← 不复制！
kfree_skb()        (skb destructor puts page)
                   page 返回 pool（不释放）
```

page_pool 的本质：**分配一批 page 存入池子，RX 帧到来时从池子取 page，用 `build_skb()` 包装，处理完后 page 返回池子而非释放**。

---

## 2. page_pool 生命周期调用链

### 2.1 page_pool 创建（每队列独立）

```
stage11_create_page_pool()
└── page_pool_create(&params)
    ├── 创建 struct page_pool
    ├── 分配 struct ptr_ring（容量 = pool_size）
    ├── 分配 alloc cache（PP_ALLOC_CACHE_SIZE = 128）
    └── 返回 struct page_pool *
```

### 2.2 page_pool 销毁

```
stage11_destroy_page_pool()
└── page_pool_destroy(pool)
    ├── ptr_ring 清理（未回收的 page 泄漏警告）
    ├── 取消 delayed_work
    └── 释放 struct page_pool
```

### 2.3 page_pool 分配 page（refill）

```
stage11_refill_rx_slot()
└── page_pool_dev_alloc_pages(q->pp, GFP_ATOMIC)
    └── page_pool_alloc_pages(pool, gfp)
        ├── [slow path] alloc_pages()
        └── [fast path] 从 alloc cache 弹出
```

### 2.4 page_pool 回收 page（TX slot FREE 时）

```
stage11_complete_tx_one()
└── kfree(s->buf)          ← TX bounce buffer 释放
    slot->page = NULL       ← TX 不持有 page
```

> 注：TX 使用 bounce buffer（kmalloc），不经过 page_pool。

### 2.5 page_pool 回收 page（RX consume 成功时）

```
stage11_consume_rx_one()
└── netif_receive_skb(skb)
    └── (skb destructor: skb_release_data)
        └── put_page(page)         ← refcount: 1→0，page 返回 pool
```

### 2.6 page_pool 回收 page（RX consume 失败时）

```
stage11_consume_rx_one()  [build_skb 失败]
└── page_pool_recycle_direct(q->pp, page)
    └── page_pool_put_full_page(pool, page, true)
        └── page_pool_put_page(pool, page, -1, true)
            └── [PP_FLAG_DMA_SYNC_DEV] dma_sync...
            └── put_page(page)     ← refcount: 1→0，page 返回 pool
```

---

## 3. napi_build_skb() vs build_skb() 深度对比

### 3.1 build_skb() 内部实现（Linux 5.15 源码）

```c
// net/core/skbuff.c
struct sk_buff *__build_skb(void *data, unsigned int frag_size)
{
    struct sk_buff *skb = kmem_cache_alloc(skbuff_head_cache, GFP_ATOMIC);
    if (unlikely(!skb))
        return NULL;
    memset(skb, 0, offsetof(struct sk_buff, tail));
    __build_skb_around(skb, data, frag_size);
    return skb;
}

static void __build_skb_around(struct sk_buff *skb, void *data, unsigned int frag_size)
{
    struct skb_shared_info *shinfo;
    skb->head = data;
    skb->data = data;
    skb_reset_tail_pointer(skb);
    skb->end = skb->tail + size;
    shinfo = skb_shinfo(skb);
    memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
    atomic_set(&shinfo->dataref, 1);   // 注意：初始化为 1
    skb->destructor = &skb_release_data; // 关键！
}
```

**关键**：`build_skb` **不调用 `get_page`**！

### 3.2 napi_build_skb() 内部实现

```c
// net/core/skbuff.c
struct sk_buff *napi_build_skb(void *data, unsigned int frag_size)
{
    struct sk_buff *skb = __napi_build_skb(data, frag_size);
    if (likely(skb) && frag_size) {
        skb->head_frag = 1;
        skb_propagate_pfmemalloc(virt_to_head_page(data), skb);
        // ↑ skb_propagate_pfmemalloc 内部调用 get_page()
    }
    return skb;
}
```

### 3.3 skb_release_data() — skb destructor

```c
// net/core/skbuff.c
void skb_release_data(struct sk_buff *skb)
{
    struct page *page = skb->head;
    struct skb_shared_info *shinfo = skb_shinfo(skb);

    if (shinfo->dataref.counter-- > 1)
        return;                         // 还有引用，不释放
    put_page(page);                     // dataref==1 时才 put_page
    kfree(skb);
}
```

### 3.4 完整 refcount 变化对比

#### build_skb() 路径（page_pool 使用场景）

```
page_pool_alloc_pages() → page (refcount=1)
    ↓
build_skb(page_address(page)) → skb
    - skb->head = page
    - shinfo->dataref = 1
    - NO get_page() called
    ↓
netif_receive_skb(skb)
    ↓
kfree_skb(skb) → skb_release_data
    ↓
shinfo->dataref.counter--  (1→0)
    ↓
put_page(page) → refcount: 1→0 → page 返回 page_pool ✓
```

#### napi_build_skb() 路径

```
page_pool_alloc_pages() → page (refcount=1)
    ↓
napi_build_skb(page_address(page))
    - skb->head = page
    - skb_propagate_pfmemalloc() → get_page(page)
    - refcount: 1→2
    ↓
netif_receive_skb(skb)
    ↓
kfree_skb(skb) → skb_release_data
    ↓
shinfo->dataref.counter--  (2→1)
    ↓
return (dataref > 1，不释放 page)
    ↓
...later... kfree_skb 再次触发
    ↓
shinfo->dataref.counter--  (1→0)
    ↓
put_page(page) → refcount: 1→0 → page 返回 page_pool ✓
```

### 3.5 两种函数对比表

| 维度 | `build_skb()` | `napi_build_skb()` |
|------|--------------|-------------------|
| get_page 调用 | **否** | **是**（via `skb_propagate_pfmemalloc`） |
| 适用场景 | page_pool / 已知 refcount=1 | 独立分配的 page |
| destructor 释放次数 | 1 次 put_page | 2 次 put_page（需两次 kfree_skb） |
| shinfo->dataref 初始化 | 1 | 1（但 get_page 使 page refcount=2） |
| 内存开销 | 低（无额外 get_page） | 高（atomic 操作） |
| page_pool 场景 | ✅ 推荐 | ⚠️ 需注意 refcount |

---

## 4. stage11 全链路调用链

### 4.1 TX 路径（ndo_start_xmit → backend）

```
用户空间发送 skb
    ↓
dev_queue_xmit(skb)
    ↓
ndo_start_xmit(skb, ndev)  [stage11_start_xmit]
    ├── skb_get_queue_mapping(skb) → qid
    ├── kmalloc(buf)              ← bounce buffer（soft 模型无 DMA）
    ├── memcpy(buf, skb->data)    ← 数据复制到 bounce buffer
    ├── slot->state = S11_SLOT_SUBMITTED
    └── stage11_mark_doorbell(q)  ← 触发 backend work
        └── queue_work(backend_wq, &q->backend_work)

[backend_workfn 在工作队列中异步执行]
    ↓
stage11_backend_workfn()
    ├── memcpy(rxs->buf, txs->buf) ← 模拟 TX→RX 转发
    ├── rxs->state = S11_SLOT_READY
    └── stage11_raise_irq(q)
        └── queue_work(irq_wq, &q->irq_work)
```

### 4.2 中断模拟路径（irq_work → NAPI poll）

```
stage11_raise_irq()
└── queue_work(irq_wq, &q->irq_work)

[irq_workfn 在 irq_wq 中执行]
    ↓
stage11_irq_workfn()
    ├── atomic64_inc(vec->handle_count)
    └── __napi_schedule(&q->napi)
        └── __netif_napi_schedule()
            └── __napi_schedule_prep() → napi_schedule()

[ksoftirqd 线程中执行]
    ↓
stage11_napi_poll(napi, budget)
    ├── spin_lock(state_lock)
    ├── [TX complete] stage11_complete_tx_one() × N
    │   └── kfree(s->buf) ← TX bounce buffer 释放
    └── [RX consume] stage11_consume_rx_one() × N
        ├── napi_build_skb(buf)
        ├── skb_put(skb, len)
        ├── netif_receive_skb(skb)
        ├── slot 清理
        └── stage11_refill_rx_slot() ← 立即 refill
    └── napi_complete_done()
```

### 4.3 RX slot 状态机详细流转

```
        ┌─────────────────────────────────────────────┐
        │                                             │
        ▼                                             │
    FREE ──[refill]──► POSTED ──[backend]──► READY ──[napi_poll consume]──► DONE ──► FREE
                                   │                                    ▲
                                   │                                    │
                                   └─────── [refill after consume] ─────┘

详细：
slot[N].state = S11_SLOT_FREE
    ↓ stage11_refill_rx_slot()  [page_pool_dev_alloc_pages]
    slot[N].state = S11_SLOT_POSTED
    slot[N].page = alloced_page
    slot[N].buf = page_address(page)
    q.rx_posted++
    ↓ backend_workfn  [memcpy TX→RX]
    slot[N].state = S11_SLOT_READY
    slot[N].data_len = copy_len
    q.rx_posted--, q.rx_ready++
    ↓ stage11_consume_rx_one()  [napi_build_skb → netif_receive_skb]
    slot[N].state = S11_SLOT_DONE
    q.rx_ready--
    ↓ stage11_refill_rx_slot()  [立即补充新 page]
    slot[N].page = NULL (旧 page 已由 skb destructor 持有)
    slot[N].state = S11_SLOT_FREE
    ↓ [下一轮 refill]
    slot[N].page = new_alloced_page
    slot[N].state = S11_SLOT_POSTED
```

---

## 5. page_pool 与 DMA sync

### 5.1 PP_FLAG_DMA_SYNC_DEV 标志

`PP_FLAG_DMA_SYNC_DEV` 告诉 page_pool：分配的 page 将用于 DMA 设备写入，CPU 读取前需要同步。

```
创建 page_pool 时设置：
struct page_pool_params params = {
    .flags = PP_FLAG_DMA_SYNC_DEV,
    ...
};

分配 page 后（DMA 设备写入数据）：
page_pool_alloc_pages() → page (已 DMA 映射)
    ↓ [DMA 设备写入数据到 page]
    ↓
CPU 读取前必须同步：
page_pool_dma_sync_for_cpu(pool, page, ...)
    └── dma_sync_single_range_for_device()
    ↓
build_skb(page_address(page))
    ↓
netif_receive_skb(skb)
```

### 5.2 soft 模型中的 DMA sync

soft 版本没有真实 DMA 设备，不设置 `PP_FLAG_DMA_SYNC_DEV`，因此：
- `page_pool_dev_alloc_pages()` 直接 `alloc_pages()`，无 DMA 映射
- 不需要 `page_pool_dma_sync_for_cpu()`
- page 生命周期完全由 refcount 管理

---

## 6. stage11 与 stage11test 方案对比

| 维度 | stage11（当前实现） | stage11test（参考实现） |
|------|---------------------|------------------------|
| 数据上送 | `napi_build_skb`（零拷贝） | `napi_alloc_skb` + `memcpy`（copy-to-skb） |
| page 回收 | destructor 隐式 `put_page` | `page_pool_put_full_page` 显式回收 |
| page refcount | 复杂（get_page×2, put_page×2） | 简单（1→0 直接归池） |
| 性能 | 更高（零拷贝） | 略低（一次 memcpy） |
| 正确性 | ⚠️ `Bad page state` 警告 | ✅ 无警告 |
| 学习价值 | 理解零拷贝路径 | 理解 page_pool 基本用法 |

### 6.1 stage11test consume 路径

```c
// stage11test 的 consume（copy-to-skb 方案）
static int stage11_consume_rx_one(struct stage11_queue *q)
{
    // 1. 分配新 skb
    skb = napi_alloc_skb(&q->napi, len + NET_IP_ALIGN);
    if (!skb) {
        atomic64_inc(&q->stats.rx_dropped);
        return 0;
    }
    // 2. 复制数据
    skb_reserve(skb, NET_IP_ALIGN);
    memcpy(skb_put(skb, len), slot->va, len);
    // 3. 上送协议栈
    netif_receive_skb(skb);
    // 4. 显式回收 page（不依赖 destructor）
    stage11_release_rx_page(q, slot);
    // 5. 立即 refill
    stage11_refill_rx_slot(q, q->ring.consume_idx);
    return 1;
}

static void stage11_release_rx_page(struct stage11_queue *q, struct stage11_rx_slot *slot)
{
    page_pool_put_full_page(q->pp, slot->page, false);
    slot->page = NULL;
    slot->va = NULL;
    slot->state = S11_SLOT_FREE;
}
```

---

## 7. 常见错误

### 7.1 在 build_skb 成功后调用 page_pool_recycle_direct

```
错误：
skb = build_skb(page);           // refcount 不变（无 get_page）
netif_receive_skb(skb);
page_pool_recycle_direct(pp, page); // 错误：refcount 从 1→0
// skb destructor 再 put_page → refcount 0→? → 双重释放

正确（使用 build_skb）：
// 不调用 recycle，destructor 自动处理
netif_receive_skb(skb);  // destructor put_page 归池

正确（使用 napi_build_skb）：
// 不调用 recycle，destructor 自动处理
netif_receive_skb(skb);  // 两次 put_page 归池
```

### 7.2 混淆 build_skb 和 napi_build_skb

- `build_skb`：refcount 不变，destructor 放一次 page
- `napi_build_skb`：`get_page` 一次，destructor 放两次 page（需要两次 kfree_skb）

在 page_pool 场景下，**推荐用 `build_skb`**（假设 page refcount 已是 1），代码更简洁。

### 7.3 在 slot 中同时存 skb 和 page

```
错误设计：
struct slot {
    struct sk_buff *skb;  // ← build_skb 后 skb 和 page 共享
    struct page *page;    // ← 同一个 page，冗余且容易出错
};
```

正确设计（stage11）：
```c
struct slot {
    struct page *page;    // 持有 page（核心）
    void *buf;           // page_address(page)，用于 bounce copy
};
```

`build_skb` 从 page 构建 skb，两者共享同一 page，不冗余。

### 7.4 page_pool_recycle_direct vs page_pool_put_page

| 函数 | 行为 | 适用场景 |
|------|------|---------|
| `page_pool_recycle_direct` | 强制 recycle 到 pool（可能降级为 `put_page`） | 确定 page 不再被使用 |
| `page_pool_put_page` | 标准的 page 归池（考虑 DMA sync、fragment 等） | 一般回收场景 |

在 `build_skb` 失败路径中，page refcount=1 未被使用，两者效果相同。但在更复杂的场景下应使用 `page_pool_put_page`。

---

## 8. page_pool 关键统计指标

### 8.1 debugfs page_pool 输出解读

```
q0: pool=000000001b1b3c43 pp_alloc=176 pp_recycle=0 pp_build_skb_fail=0 posted=127 ready=0
```

| 字段 | 含义 | 正常值 |
|------|------|--------|
| `pp_alloc` | `page_pool_dev_alloc_pages` 成功次数 | 持续增长 |
| `pp_recycle` | `page_pool_put_page` 触发次数 | 0（依赖 destructor 时） |
| `pp_build_skb_fail` | `build_skb()` 失败次数 | 0 |
| `posted` | 当前 POSTED 状态 slot 数 | ≈ ring_size |
| `ready` | 当前 READY 状态 slot 数 | 0 或少量 |

### 8.2 平衡性检查

```
pp_alloc ≈ rx_consume + posted + ready + pp_build_skb_fail

解释：
- 每个 consum 后 slot 立即 refill，新 page 计入 pp_alloc
- posted + ready = 当前在用 page 数
- pp_build_skb_fail = build_skb 失败数（应接近 0）
- pp_recycle 非 0 表示有显式 recycle 路径被触发
```

---

## 9. 参考资料

- `net/core/page_pool.c` — page_pool 核心实现
- `net/core/skbuff.c` — `build_skb()`, `napi_build_skb()`, `skb_release_data()`
- `include/net/page_pool.h` — page_pool API 定义
- `Documentation/networking/page_pool.rst` — kernel 官方文档
- virtio-net, igb, mlx5 驱动源码 — 真实驱动参考实现
