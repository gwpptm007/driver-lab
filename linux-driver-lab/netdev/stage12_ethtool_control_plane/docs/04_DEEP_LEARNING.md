# 04_DEEP_LEARNING — ethtool 与 page_pool 核心调用链

## 1. ethtool 系统调用链

### 1.1 用户空间 → 内核路径

```
用户: ethtool -S nds12s
    ↓
sys_ioctl()
    ↓
sock_ioctl()
    ↓
dev_ioctl()
    ↓
ethtool_ioctl()
    ↓
ethtool_get_stats()
    ↓
dev_get_sset_count()  → get_sset_count(ETH_SS_STATS)
dev_get_strings()      → get_strings(ETH_SS_STATS, buf)
dev_get_ethtool_stats() → get_ethtool_stats(stats, data)
```

### 1.2 ethtool_ops 调用流程

```
ethtool -i nds12s
└── ethtool_ioctl()
    └── dev_get_drvinfo()
        └── stage12_get_drvinfo()

ethtool -S nds12s
└── ethtool_ioctl()
    ├── dev_get_sset_count(ETH_SS_STATS) → stage12_get_sset_count()
    ├── dev_get_strings(ETH_SS_STATS)      → stage12_get_strings()
    └── dev_get_ethtool_stats()           → stage12_get_ethtool_stats()

ethtool -G nds12s
└── ethtool_ioctl()
    └── ethtool_get_ringparam()
        └── stage12_get_ringparam()

ethtool -L nds12s
└── ethtool_ioctl()
    └── ethtool_get_channels()
        └── stage12_get_channels()
```

---

## 2. stage12_get_drvinfo 完整调用

```
ethtool -i nds12s
    ↓
struct ethtool_drvinfo drvinfo;
ethtool_get_drvinfo(ndev, &drvinfo);
    ↓
stage12_get_drvinfo(ndev, &drvinfo)
{
    strscpy(drvinfo.driver, "netdev_stage12", ...);
    strscpy(drvinfo.version, "1.0", ...);
    strscpy(drvinfo.bus_info, "platform", ...);
}
    ↓
用户空间打印:
  driver: netdev_stage12
  version: 1.0
  bus-info: platform
```

---

## 3. stage12_get_ethtool_stats 完整调用链

```
ethtool -S nds12s
    ↓
/* 第一次调用：获取统计项数量 */
stage12_get_sset_count(ndev, ETH_SS_STATS)
    → return STAGE12_ETHTOOL_STATS_COUNT;  // = 13

/* 第二次调用：获取统计名称 */
stage12_get_strings(ndev, ETH_SS_STATS, buf)
    → memcpy(buf, stage12_ethtool_stat_names, sizeof(...));
    → 用户显示:
        tx_packets
        tx_bytes
        tx_submit_count
        ...

/* 第三次调用：获取统计值 */
stage12_get_ethtool_stats(ndev, stats, data)
    ↓
memset(data, 0, sizeof(u64) * STAGE12_ETHTOOL_STATS_COUNT);
    ↓
for (i = 0; i < priv->num_queues; i++) {
    struct stage12_queue *q = &priv->queues[i];
    data[STAGE12_ETHTOOL_STATS_TX_PACKETS] += atomic64_read(&q->stats.tx_packets);
    data[STAGE12_ETHTOOL_STATS_TX_BYTES]   += atomic64_read(&q->stats.tx_bytes);
    // ... 累加所有队列
}
    ↓
用户显示:
     tx_packets: 88
     tx_bytes: 14148
     ...
```

---

## 4. page_pool 生命周期调用链（stage12 继承自 stage11）

### 4.1 page_pool 创建

```
stage12_init()
    ↓
for (i = 0; i < priv->num_queues; i++) {
    q->pp = stage12_create_page_pool(q);
}
    ↓
stage12_create_page_pool(q)
└── page_pool_create(&params)
    ├── 创建 struct page_pool
    ├── 分配 struct ptr_ring（pool_size 容量）
    ├── 分配 alloc cache（PP_ALLOC_CACHE_SIZE = 128）
    └── 返回 struct page_pool *
```

### 4.2 RX slot refill（page_pool 分配）

```
stage12_backend_workfn()
    ↓
stage12_refill_rx_slot(q, idx)
    ↓
page = page_pool_alloc_pages(q->pp, GFP_ATOMIC)
    ↓
s->page = page;
s->buf = page_address(page);
s->state = S12_SLOT_POSTED;
s->data_len = 0;
q->rx_posted++;
```

### 4.3 RX consume（build_skb + destructor 回收）

```
stage12_napi_poll()
    ↓
stage12_consume_rx_one(q)
    ↓
/* 1. 从 page 构建 skb（零拷贝） */
skb = build_skb(s->buf, priv->rx_buf_size);
    ↓
/* 2. 上送协议栈 */
skb_put(skb, len);
netif_receive_skb(skb);
    ↓
/* 3. skb destructor 自动回收 page */
skb_release_data(skb)
    ↓
put_page(page);  // refcount: 1→0，page 返回 pool
    ↓
/* 4. 立即 refill */
stage12_refill_rx_slot(q, idx);
```

### 4.4 build_skb 失败时的回收

```
build_skb() → NULL
    ↓
page_pool_recycle_direct(q->pp, page);
    ↓
page_pool_put_full_page(q->pp, page, true);
    ↓
put_page(page);  // refcount: 1→0，page 返回 pool
    ↓
stage12_refill_rx_slot(q, idx);
```

---

## 5. napi_build_skb() vs build_skb() 对比

### 5.1 build_skb()（stage12 使用）

```
page_pool_alloc_pages() → page (refcount=1)
    ↓
build_skb(page_address(page))
    - skb->head = page
    - NO get_page() called
    - refcount = 1
    ↓
netif_receive_skb(skb)
    ↓
kfree_skb(skb) → skb_release_data
    ↓
put_page(page) → refcount: 1→0 → page 返回 pool ✓
```

### 5.2 napi_build_skb()

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
put_page(page) → refcount: 2→1 (不释放 page)
    ↓
... later ...
    ↓
kfree_skb(skb) → skb_release_data
    ↓
put_page(page) → refcount: 1→0 → page 返回 pool
```

### 5.3 对比表

| 维度 | `build_skb()` | `napi_build_skb()` |
|------|--------------|-------------------|
| get_page 调用 | **否** | **是** |
| 适用场景 | page_pool / 已知 refcount=1 | 独立分配的 page |
| destructor 释放次数 | 1 次 put_page | 2 次 put_page |
| page_pool 场景 | ✅ 推荐 | ⚠️ 需注意 refcount |

---

## 6. ethtool -G / -L 完整调用链

### 6.1 ethtool -G ringparam

```
ethtool -G nds12s rx 256
    ↓
ethtool_ioctl()
    ↓
ethtool_set_ringparam()
    ↓
stage12_set_ringparam(ndev, ringparam)
{
    new_size = ringparam->rx_pending;  // 256
    if (new_size > priv->ring_size)    // 128
        return -EINVAL;
    netdev_info("runtime change not supported");
    return 0;
}
```

### 6.2 ethtool -L channels

```
ethtool -L nds12s combined 4
    ↓
ethtool_ioctl()
    ↓
ethtool_set_channels()
    ↓
stage12_set_channels(ndev, channels)
{
    new_count = channels->rx_count;    // 4
    if (new_count > priv->num_queues) // 2
        return -EINVAL;
    netdev_info("runtime change not supported");
    return 0;
}
```

---

## 7. priv_flags 实现调用链

```
ethtool --show-priv-flags nds12s
    ↓
ethtool_ioctl()
    ↓
ethtool_get_priv_flags()
    ↓
stage12_get_priv_flags(ndev)
    → return priv->ethtool_priv_flags;

ethtool --set-priv-flags nds12s test on
    ↓
ethtool_ioctl()
    ↓
ethtool_set_priv_flags()
    ↓
stage12_set_priv_flags(ndev, flags)
    → priv->ethtool_priv_flags = flags;
    → return 0;
```

---

## 8. ethtool_ops 与 netdev_ops 关系

```
netdev_ops:
    .ndo_start_xmit    → skb 发送路径
    .ndo_open           → 设备打开
    .ndo_stop          → 设备关闭
    .ndo_get_stats64   → 基础统计（ip link show）
    ↓
ethtool_ops:
    .get_drvinfo       → 驱动信息
    .get_ethtool_stats → 扩展统计（ethtool -S）
    .get_ringparam     → ring 配置
    .get_channels      → channel 配置
    ↓
两者是独立的回调集合，ethtool 通过 netdev->ethtool_ops 访问
```

---

## 9. 常见错误

### 9.1 sset_count 与 strings 不匹配

```c
/* 错误 */
get_sset_count() { return 13; }     // 返回 13 项
get_strings() { memcpy(buf, names, 10); }  // 只填充 10 个

/* 正确 */
get_sset_count() { return ETHTOOL_STATS_COUNT; }
get_strings() { memcpy(buf, stage12_ethtool_stat_names,
                       sizeof(stage12_ethtool_stat_names)); }
```

### 9.2 stats 累加而非覆盖

```c
/* 错误：多次调用会累加 */
data[ETHTOOL_STATS_TX_PACKETS] += atomic64_read(&q->stats.tx_packets);

/* 正确：先 memset 为 0 */
memset(data, 0, sizeof(u64) * STAGE12_ETHTOOL_STATS_COUNT);
data[ETHTOOL_STATS_TX_PACKETS] += atomic64_read(&q->stats.tx_packets);
```

### 9.3 使用 strlcpy 而非 strscpy

```c
/* 新内核已废弃 strlcpy，应使用 strscpy */
strlcpy(drvinfo->driver, "netdev_stage12", ...);  // 编译警告
strscpy(drvinfo->driver, "netdev_stage12", ...);   // 正确
```

---

## 10. 参考资料

- `include/linux/ethtool.h` — ethtool_ops 定义
- `net/core/ethtool.c` — ethtool 核心实现
- `net/core/dev_ioctl.c` — ioctl 分发
- igb, ixgbe, virtio-net 驱动 ethtool_ops 源码