# 03_QUEUE_NAPI_FEATURES

## Queue 模型

```
virtnet_info (驱动私有结构)
  ├─ num_queue_pairs — 队列对数（TX+RX = 1对）
  ├─ sq — virtnet_sq[] — 发送队列数组
  ├─ rq — virtnet_rq[] — 接收队列数组
  └─ rq[i].napi — 对应 NAPI 结构
```

每对队列：
- `virtnet_sq[i]` — TX virtqueue
- `virtnet_rq[i]` — RX virtqueue
- 共享 `virtqueue` 后端（与 host 共享）

---

## NAPI 与 Poll

```c
struct virtnet_rq {
    struct napi_struct napi;  // 每个 RX 队列一个 NAPI
    struct virtqueue *vq;     // 绑定的 virtqueue
    void *data;               // receive_buf 相关数据
};
```

poll 路径：
- `virtnet_poll()` — NAPI 轮询回调
- `virtnet_open()` — 启用 napi_enable
- `virtnet_close()` — 禁用 napi_disable

---

## 中断与通知

| 事件 | 触发者 | 处理 |
|------|--------|------|
| RX 数据到达 | host 写 virtqueue | IRQ → schedule napi |
| TX 完成 | host 消费 virtqueue | 可能触发 IRQ 或 poll reclaim |
| Kick | 驱动主动 | `virtqueue_kick()` 通知 host |

---

## Features 与 Offload

feature bits 在 `virtnet_probe()` 中协商：

```c
/* 关键 feature */
VIRTIO_NET_F_CSUM      — checksum offload
VIRTIO_NET_F_GUEST_CSUM — guest 计算 checksum
VIRTIO_NET_F_GSO       — GSO offload
VIRTIO_NET_F_GRO       — GRO 支持
VIRTIO_NET_F_MRG_RXBUF  — 合并 buffer 支持
```

驱动侧 vs 协议栈：
- 驱动声明 `ndev->features` = 实际能力
- 协议栈通过 GRO/GSO/csum 路径做决策
- `napi_gro_receive()` vs `netif_receive_skb()` 决定是否合并

---

## Ethtool 入口

```c
static const struct ethtool_ops virtnet_ethtool_ops = {
    .get_link        = ethtool_op_get_link,
    .get_channels    = virtnet_get_channels,
    .set_channels    = virtnet_set_channels,
    .get_stats_count = virtnet_get_sset_count,
    .get_ethtool_stats = virtnet_get_ethtool_stats,
};
```

stats 通过 `ndo_get_ethtool_stats` 填充：
- `tx_bytes`, `tx_packets` — 发送统计
- `rx_bytes`, `rx_packets` — 接收统计
- 与 `ethtool -S` 输出对应

---

## XDP 入口

```c
static const struct net_device_ops virtnet_netdev_ops = {
    .ndo_start_xmit     = start_xmit,
    .ndo_bpf            = virtnet_xdp,  // XDP 入口
    .ndo_set_features   = virtnet_set_features,
};
```

XDP 处理路径（在 RX poll 内）：
```c
if (xdp_prog) {
    xdp.data = page_address(page) + off;
    act = bpf_prog_run(xdp_prog, &xdp);
    // XDP_PASS → 走正常 skb 路径
    // XDP_DROP → 直接丢弃
    // XDP_TX / XDP_REDIRECT → 快速路径
}
```

对比 stage14：
- 教学驱动有简化 XDP 入口
- 真实驱动处理更复杂的 buffer 生命周期

---

## 验证方法

通过 `lab-virtio-net-runtime-observe` 验证：
- 查看 `/sys/kernel/tracing/events/napi/napi_poll` 跟踪 napi 调度
- 对照 `ethtool -S` 统计变化