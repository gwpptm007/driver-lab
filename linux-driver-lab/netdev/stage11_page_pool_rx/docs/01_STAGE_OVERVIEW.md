# 01_STAGE_OVERVIEW — stage11 概述

## stage11 学习目标

掌握 Linux kernel `page_pool` API，理解其在高性能网络驱动中的核心地位：

1. **page_pool 生命周期**：`page_pool_create` → `page_pool_dev_alloc_pages` → `put_page` 回收
2. **`build_skb()` 零拷贝**：从 page 直接构建 skb，避免每帧 `alloc_skb`
3. **page recycling 机制**：成功路径（skb destructor 隐式）vs 失败路径（`page_pool_recycle_direct` 显式）
4. **每队列独立 pool**：真实驱动模式（virtio-net、igb、mlx5 都这样用）

## 什么是 page_pool

page_pool 是 Linux kernel 2.6.37 引入的网络驱动 RX buffer 内存管理基础设施，核心解决：

```
传统方式（stage10）：
  每帧 RX: netdev_alloc_skb() → 处理 → kfree_skb()
  问题: 1488万帧/秒 @ 10Gbps，每帧都走 alloc/free

page_pool 方式（stage11）：
  预分配一批 page 到池子
  每帧 RX: page_pool_alloc_pages() → build_skb(page) → skb destructor 自动回收
  优点: page 复用，避免频繁 alloc/free
```

## 与 stage10 的对比

| 维度 | stage10 | stage11 |
|------|---------|---------|
| RX buffer | `netdev_alloc_skb()` 每帧分配 | `page_pool_dev_alloc_pages()` 池子分配 |
| skb 构建 | 原始 skb 数据直接用 | `build_skb()` 从 page 零拷贝构建 |
| RX slot 持有 | `struct sk_buff *skb` | `struct page *page` |
| page 回收 | 无（skb 随用随分配） | `put_page` 自动归池 |
| page_pool | 无 | 每队列独立 |

## RX 数据流对比

```
stage10:
  backend → slot.skb → napi_consume → netif_receive_skb(skb)
  refill: netdev_alloc_skb() → slot.skb（每次都是新分配）

stage11:
  backend → slot.page → napi_consume → build_skb(page) → netif_receive_skb(skb)
  refill: page_pool_dev_alloc_pages() → slot.page（从池子取，skb destructor 自动归池）
```

## 真实驱动参考

- **virtio-net**：最早广泛使用 page_pool 的驱动
- **igb / ixgbe**：Intel 千兆/万兆网卡驱动
- **mlx5**：Mellanox ConnectX 网卡驱动
- 共同模式：每队列独立 `struct page_pool`，RX path 用 `build_skb()`
