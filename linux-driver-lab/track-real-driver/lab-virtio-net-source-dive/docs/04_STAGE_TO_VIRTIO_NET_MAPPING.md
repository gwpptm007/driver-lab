# 04_STAGE_TO_VIRTIO_NET_MAPPING

| 你的阶段 | 关键词 | 在 `virtio_net` 里要重点看什么 |
|---|---|---|
| `stage01_netdev_skeleton` | `alloc_etherdev` / `register_netdev` | `probe()` 中 netdev 分配、初始化与注册 |
| `stage03_napi_poll` | poll / budget / irq 抑制 | `virtnet_poll()`、预算控制、poll 与中断的协作 |
| `stage04_ring_dma` | ring / refill | virtqueue descriptor、buffer 提交、completion / refill |
| `stage09_multi_queue_scaling` | 多队列 | queue pair、CPU/NAPI/queue 的分配与伸缩 |
| `stage10_msix_per_queue_irq` | per-queue IRQ | 中断/通知组织、queue 与中断对应关系 |
| `stage11_page_pool_rx` | RX page recycle | 真实 RX buffer/page 生命周期与回收策略 |
| `stage12_ethtool_control_plane` | 控制面 | ethtool ops / stats / queue/channel 相关接口 |
| `stage13_offload_basics` | checksum / GRO / GSO | feature bits、协议栈协同 |
| `stage14_xdp_basics` | XDP 入口 | XDP attach 点、普通 skb path 与 fast path 的边界 |
