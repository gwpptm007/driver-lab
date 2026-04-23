# stage vs virtio_net report

| stage | 主题 | virtio_net 观察点 | 差异/备注 |
|---|---|---|---|
| stage01_netdev_skeleton | netdev 注册 | probe / alloc / register_netdev | 真实驱动会把 feature、queue、virtqueue 初始化放进统一骨架 |
| stage03_napi_poll | NAPI / budget | poll / napi schedule / complete | 待补充 |
| stage04_ring_dma | ring / refill | virtqueue / refill / completion | 待补充 |
| stage09_multi_queue_scaling | 多队列 | queue pair / queue select | 待补充 |
| stage10_msix_per_queue_irq | per-queue IRQ | interrupt / notify / napi | 待补充 |
| stage11_page_pool_rx | RX page recycle | page / buffer recycle | 待补充 |
| stage12_ethtool_control_plane | 控制面 | ethtool / stats / channels | 待补充 |
| stage13_offload_basics | offload | feature bits / stack boundary | 待补充 |
| stage14_xdp_basics | XDP 入口 | attach / RX fast path | 待补充 |
