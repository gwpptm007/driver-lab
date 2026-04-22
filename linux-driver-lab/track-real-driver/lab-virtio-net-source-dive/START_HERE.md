# START_HERE

## 建议阅读顺序

1. `README.md`
2. `docs/01_LAB_OVERVIEW.md`
3. `docs/02_VIRTIO_NET_ARCHITECTURE.md`
4. `docs/03_PROBE_TX_RX_READING_ORDER.md`
5. `docs/04_STAGE_TO_VIRTIO_NET_MAPPING.md`
6. `docs/05_ACCEPTANCE_AND_NEXT_STEP.md`

## 推荐阅读节奏

### 第 1 轮：建立骨架
先搞清楚：`virtio_driver`、`probe/remove`、`virtnet_info`、queue、`net_device_ops`。

### 第 2 轮：沿 TX/RX 主路径看函数
重点回答：TX 从哪里进入 virtqueue，RX 从哪里进入 NAPI poll，completion / refill 是怎么回来的。

### 第 3 轮：和自己的 stage 做映射
重点对照：`stage03_napi_poll`、`stage09_multi_queue_scaling`、`stage10_msix_per_queue_irq`、`stage11_page_pool_rx`、`stage12_ethtool_control_plane`、`stage13_offload_basics`、`stage14_xdp_basics`。
