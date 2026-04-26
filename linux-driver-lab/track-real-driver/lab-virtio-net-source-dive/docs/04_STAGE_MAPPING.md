# 04_STAGE_MAPPING

## 快速对照表

| stage | 关键词 | 在 virtio_net 里对应 | 差异 |
|-------|--------|---------------------|------|
| stage01 | netdev 注册 | `virtnet_probe()` 中 `alloc_etherdev` + `register_netdev` | 真实驱动同时初始化 feature/queue |
| stage02 | skb 路径 | `start_xmit()` 的 skb 处理 | 真实驱动处理 GSO 分片和 offload 判断 |
| stage03 | napi_poll | `virtnet_poll()` / `napi_struct` | 每 RX 队列独立 napi，与 virtqueue 耦合 |
| stage04 | ring/dma | `virtqueue_add_outbuf()` / `virtqueue_get_buf()` | virtqueue 是与 host 共享的环，不是纯 DMA |
| stage05-07 | char/平台 | probe/remove 骨架 | 真实驱动有完整的 feature negotiation |
| stage08 | 中断/IRQ | MSI-X / per-queue IRQ 配置 | 多队列与 IRQ affinity 绑定 |
| stage09 | 多队列 | `num_queue_pairs` 多对 TX+RX | 队列组织更复杂 |
| stage10 | MSI-X | `pci_enable_msix()` / vector 分配 | 和 q queue 分配策略关联 |
| stage11 | page_pool | RX buffer page 管理 | recycle/refill 机制比教学复杂 |
| stage12 | ethtool | `ndo_get_ethtool_stats` / `ethtool_ops` | stats 丰富，channels 可动态调整 |
| stage13 | offload | feature bits + `napi_gro_receive()` | GRO 和 checksum 是 feature negotiation 结果 |
| stage14 | XDP | `ndo_bpf` / `xdp_buff` | 与 GRO/skbb path 有交叉，处理更完整 |

---

## 核心映射

### TX 路径
```
stage02 (skb) → start_xmit()
stage04 (ring) → virtqueue_add_outbuf()
stage13 (offload) → checksum/GSO 判断
```

### RX 路径
```
stage11 (page_pool) → receive_buf() + build_skb()
stage03 (napi) → virtnet_poll()
stage13 (offload) → gro_receive() / netif_receive_skb()
stage14 (XDP) → XDP 判断在 build_skb 之前
```

---

## 关键结论

1. **教学驱动是真实驱动的简化版** — 真实驱动处理更多边界情况
2. **feature negotiation 是核心** — stage13 的 offload 不是简单开关
3. **XDP 和 GRO 可以交叉** — stage14 只是简化入口
4. **多队列模型一致** — 概念上与 stage09 相同