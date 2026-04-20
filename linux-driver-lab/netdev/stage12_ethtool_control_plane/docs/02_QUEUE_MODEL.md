# 02_QUEUE_MODEL — stage12 架构与 ethtool 控制面

## 核心架构

stage12 在 stage11 RX page_pool 基础上新增 **ethtool 控制面**：

```
用户空间                  驱动                      内核
   │                       │                        │
   │ ethtool -i nds12s    │                        │
   │ ───────────────────> │ get_drvinfo            │
   │ <─────────────────── │ 驱动信息                │
   │                       │                        │
   │ ethtool -S nds12s    │                        │
   │ ───────────────────> │ get_strings            │
   │ <─────────────────── │ stat names             │
   │ ───────────────────> │ get_sset_count         │
   │ <─────────────────── │ stat count             │
   │ ───────────────────> │ get_ethtool_stats      │
   │ <─────────────────── │ stat values            │
   │                       │                        │
   │ ethtool -G nds12s    │                        │
   │ ───────────────────> │ get_ringparam          │
   │ <─────────────────── │ ring_size=128          │
   │                       │                        │
   │ ethtool -L nds12s    │                        │
   │ ───────────────────> │ get_channels           │
   │ <─────────────────── │ rx_count=2, tx_count=2 │
```

---

## ethtool_ops 实现

stage12 实现了完整的 `ethtool_ops`：

```c
static const struct ethtool_ops stage12_ethtool_ops = {
    .get_drvinfo        = stage12_get_drvinfo,
    .get_strings        = stage12_get_strings,
    .get_sset_count     = stage12_get_sset_count,
    .get_ethtool_stats  = stage12_get_ethtool_stats,
    .get_ringparam      = stage12_get_ringparam,
    .set_ringparam      = stage12_set_ringparam,
    .get_channels       = stage12_get_channels,
    .set_channels       = stage12_set_channels,
    .get_priv_flags     = stage12_get_priv_flags,
    .set_priv_flags     = stage12_set_priv_flags,
    .get_link           = ethtool_op_get_link,
};
```

注册到 netdev：
```c
ndev->ethtool_ops = &stage12_ethtool_ops;
```

---

## 统计数据导出流程

`ethtool -S` 需要三个回调配合：

```
1. get_strings: 返回统计名称数组
2. get_sset_count: 返回统计项数量
3. get_ethtool_stats: 返回统计值数组
```

```c
// 1. 统计名称（固定字符串数组）
static const char stage12_ethtool_stat_names[][ETH_GSTRING_LEN] = {
    [STAGE12_ETHTOOL_STATS_TX_PACKETS]  = "tx_packets",
    [STAGE12_ETHTOOL_STATS_TX_BYTES]    = "tx_bytes",
    // ...
};

// 2. 统计计数
static int stage12_get_sset_count(struct net_device *ndev, int sset)
{
    if (sset == ETH_SS_STATS)
        return STAGE12_ETHTOOL_STATS_COUNT;  // = 13
    return -EOPNOTSUPP;
}

// 3. 统计值（从队列累加）
static void stage12_get_ethtool_stats(struct net_device *ndev,
                                      struct ethtool_stats *stats, u64 *data)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;
    memset(data, 0, sizeof(u64) * STAGE12_ETHTOOL_STATS_COUNT);
    for (i = 0; i < priv->num_queues; i++) {
        struct stage12_queue *q = &priv->queues[i];
        data[STAGE12_ETHTOOL_STATS_TX_PACKETS] += atomic64_read(&q->stats.tx_packets);
        // ... 累加所有统计
    }
}
```

---

## 核心设计：每队列独立 page_pool

stage12 继续使用**每队列独立 page_pool**：

```c
struct stage12_queue {
    ...
    struct page_pool *pp;  // 每个队列独立
};
```

---

## RX slot 结构（不变）

```c
struct stage12_buf_slot {
    struct page *page;     // 来自 page_pool 的 page
    void *buf;            // page_address(page)
    u16 buf_len;
    u16 data_len;
    enum stage12_slot_state state;
    u16 id;
};
```

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
| `S12_SLOT_FREE` | slot 空闲，无 page |
| `S12_SLOT_POSTED` | page 已分配（来自 page_pool），等待 backend 填充 |
| `S12_SLOT_READY` | backend 已将 TX 数据复制到 page，数据就绪 |
| `S12_SLOT_DONE` | napi 已消费 |

---

## build_skb() 零拷贝路径

| 函数 | 特点 | 适用场景 |
|------|------|----------|
| `build_skb()` | 不额外 get_page，destructor 只 put_page 一次 | page_pool 场景 |
| `napi_build_skb()` | 额外 get_page，destructor put_page 两次 | 独立分配的 page |

stage12 使用 `build_skb()`。

---

## page_pool 与 DMA sync

`PP_FLAG_DMA_SYNC_DEV` 标志要求驱动在将 page 提供给硬件之前同步：

soft 版本不需要真实 DMA 同步，但这个标志说明了为什么高性能驱动需要 page_pool——它统一管理了 DMA coherent buffer 的生命周期。

---

## ethtool vs debugfs

| 接口 | 用途 | 特点 |
|------|------|------|
| **ethtool** | 标准 Linux 工具 | 所有 NIC 都支持，可被管理工具调用 |
| **debugfs** | 驱动私有调试 | 非标准，仅用于开发调试 |

真实驱动两者都实现：
- ethtool：标准接口，承载管理面
- debugfs：扩展统计，承载开发调试信息