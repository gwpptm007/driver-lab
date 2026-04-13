# stage04 vs virtio-net 对照报告

- stage04 driver: /home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage04_ring_dma/driver/netdev_stage04.c
- stage04 visible: yes
- virtio-net source: /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src/drivers/net/virtio_net.c

## northbound 继续保留

- README / docs / records / output 的阶段化组织
- make report / smoke 统一入口
- NAPI / RX refill / queue depth / budget 的观测口径

## southbound 必须替换

| stage04 教学实现 | virtio-net / vring 对应层 | 结论 |
|---|---|---|
| struct tx_desc / rx_desc | virtqueue / vring | 不能原样照抄 |
| owner/state 显式字段 | avail / used ring | 要从字段思维切到 ring 协议思维 |
| memcpy 模拟 device copy | 真实 buffer 提交与完成 | 教学模型结束 |
| dma_map_single/unmap 显式路径 | transport + sg + DMA 抽象 | 不能按 stage04 代码形状去找 |
| refill_rx_slot | try_fill_recv 等逻辑 | 核心问题不变 |
