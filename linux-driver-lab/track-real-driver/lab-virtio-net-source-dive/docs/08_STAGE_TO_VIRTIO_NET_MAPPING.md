# 08_STAGE_TO_VIRTIO_NET_MAPPING

| 你的阶段 | 关键词 | 在 `virtio_net` 里要重点看什么 | 差异/备注 |
|---|---|---|---|
| `stage01_netdev_skeleton` | `alloc_etherdev` / `register_netdev` | `probe()` 中 netdev 分配、初始化与注册 | 真实驱动会把 feature、queue、virtqueue 初始化一起组织 |
| `stage03_napi_poll` | poll / budget / 中断协作 | poll 入口、budget 消费、poll 完成后的重置 | 真实驱动的 poll 会和 virtqueue、queue state 深度耦合 |
| `stage04_ring_dma` | ring / refill / completion | virtqueue descriptor、buffer 提交、completion / refill | 真实模型不是教学 ring，而是 virtqueue + 协同边界 |
| `stage09_multi_queue_scaling` | 多队列 | queue pair、CPU/NAPI/queue 的分配与伸缩 | 多队列不只是数量增加，还涉及对象组织方式 |
| `stage10_msix_per_queue_irq` | per-queue IRQ | interrupt / notify / callback / napi 关系 | 真实驱动更强调中断与 poll 的配合边界 |
| `stage11_page_pool_rx` | RX recycle | 真实 RX buffer/page 生命周期 | 真实路径中 recycle/refill 的位置更复杂 |
| `stage12_ethtool_control_plane` | 控制面 | ethtool ops / stats / channels | 工业驱动的控制面远比教学驱动厚 |
| `stage13_offload_basics` | checksum / GRO / GSO | feature bits 与协议栈协同 | 真实驱动能力开关更依赖 negotiation |
| `stage14_xdp_basics` | XDP 入口 | attach 点、普通 skb path 与 fast path 的边界 | 真实驱动要处理更完整的生命周期和能力限制 |

## 建议用法

这一篇先作为总表。  
后续每完成一轮阅读，就把“差异/备注”从“待补充”逐渐写实。
