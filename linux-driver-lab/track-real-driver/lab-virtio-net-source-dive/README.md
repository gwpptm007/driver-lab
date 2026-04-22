# lab-virtio-net-source-dive

> 所属：`track-real-driver/`

## 一句话定位

这不是“再写一个新驱动”，而是：

> **把你已经完成的教学型 netdev 主线，与 Linux 内核中的真实 `virtio_net` 驱动建立映射。**

## 为什么第一个选 virtio_net

- 你已经做过 `net_device` / `skb` / NAPI / ring / multi-queue / MSI-X / page_pool / ethtool / offload / XDP
- `virtio_net` 正好可以把这些知识放回真实驱动里重新观察
- 它比一上来读 `mlx5` 更适合作为真实驱动的第一站

## 本 Lab 要回答的核心问题

1. `virtio_net` 的整体骨架是什么？
2. `probe/remove` 和 `net_device` 注册怎么组织？
3. TX/RX 路径在真实源码中如何流动？
4. NAPI、queue、virtqueue、interrupt/notify 是怎样关联的？
5. feature negotiation、ethtool、XDP 入口在哪里？
6. 它和 `netdev/stage00~stage14` 的映射关系是什么？
