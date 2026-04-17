# STAGE_OVERVIEW

## stage05 目标与边界

Stage05 要完成两件事：

1. 做一次 `stage04 ↔ virtio-net` 的职责对照
2. 做一次平台参数化准备

本阶段不追求：

- 直接改写成新的 virtio 驱动
- 一步到位做 ARM64 实迁
- 把 tap 当唯一设备模型

---

## 核心理解

**一句话总结**：stage05 不是"重写驱动"，而是把 stage04 的教学坐标系升级成能与 `virtio-net` 对话、并能迁移到多平台的工程化坐标系。

---

## virtio-net 源码阅读地图

### 推荐阅读顺序

1. `drivers/net/virtio_net.c`
2. `drivers/virtio/virtio_ring.c`
3. `include/uapi/linux/virtio_net.h`

### 第一口先抓

- `virtnet_probe`
- `virtnet_open`
- `virtnet_close`
- `virtnet_xmit`
- `virtnet_poll`
- `try_fill_recv`

---

## 概念对照

| stage04 教学概念 | virtio-net / vring 对应层 | 说明 |
|---|---|---|
| `tx_ring / rx_ring` | send / receive virtqueue | 都是在表达 buffer 提交与完成 |
| `owner/state` | avail / used ring | 都是在表达 ownership |
| `raise_irq + napi_schedule` | callback / notify / napi schedule | 都是在表达完成事件驱动 poll |
| `refill_rx_slot` | `try_fill_recv` 等补充逻辑 | 都是在解决 RX buffer 不断粮 |
| `dma_map_single/unmap` | transport + sg + DMA 抽象 | stage04 显式化，virtio 分层化 |

---

## 平台参数化

从 stage05 起统一使用：

- `TARGET_ARCH`
- `RUN_MODE`
- `HOST_CC`
- `CROSS_COMPILE`
- `QEMU_BIN`
- `KERNEL_SOURCE_ROOT`
- `KERNEL_BUILD_DIR`
- `KERNEL_IMAGE`
- `ROOTFS_IMAGE`
- `VIRTIO_NET_SOURCE`

### 推荐组合

| profile | arch | run mode | 说明 |
|---------|------|----------|------|
| host | host | host | 快速检查 |
| qemu-x86_64 | x86_64 | qemu-x86_64 | QEMU 链路走通 |
| qemu-arm64 | arm64 | qemu-arm64 | 完成 ARM64 迁移 |

---

## 与 stage04 / stage06 的关系

- **stage04**：ring / DMA / RX replenishment 教学模型
- **stage05**：virtio-net 源码对照 + 平台参数化准备
- **stage06**：ARM64 跨平台迁移收口

```
stage04（教学模型）                    stage05/virtio-net（真实实现）
─────────────────────────────────      ─────────────────────────────────
单文件 driver/netdev_stage04.c         drivers/net/virtio_net.c（1600+ 行）
├─ 单一 tx_ring / rx_ring 数组         ├─ send_queue / receive_queue + virtqueue
├─ owner/state 显式字段管理             ├─ avail_idx / used_idx ring 协议
├─ dma_map_single 每包显式映射         ├─ virtio transport 隐藏 DMA 细节
├─ raise_irq + napi_schedule           ├─ virtqueue callback → napi_schedule
├─ refill_rx_slot 同步在 poll 内        ├─ try_fill_recv 批量异步填充
└─ memcpy 模拟 device 行为             └─ 真实 virtio device（QEMU 模拟）
```

---

## 五维对照总结

### 1. RX Replenishment
```
stage04:  poll() 内每处理一个包，立刻 refill 对应 slot
virtio:   try_fill_recv() 在 poll 结束后批量填充
```

### 2. TX 路径
```
stage04:  ndo_start_xmit → skb_linearize → dma_map_single → memcpy → DONE
virtio:  ndo_start_xmit → virtio_net_hdr_from_skb → virtqueue_add_outbuf → virtqueue_notify
```

### 3. DMA 操作
```
stage04:  driver 显式调用 dma_map_single/unmap_single
virtio:  virtqueue_add_* → virtio_ring.c 处理 sg list → transport 层 DMA
```

### 4. NAPI Poll
```
stage04:  单一 napi_struct，轮询单一 rx_ring
virtio:  每个 virtqueue 一个 napi_struct（send_queue + receive_queue）
```

### 5. ownership 协议
```
stage04:  owner = CPU | DEV  显式字段
virtio:  avail->idx（driver 写）vs used->idx（device 写）环形协议
```
