# Netdev Evidence

## 对应章节

- `../docs/01_KERNEL_NETDEV_PATH.md`

## 主入口

- `../../netdev/README.md`

## 关键阶段

```text
stage00_bootstrap
stage01_netdev_skeleton
stage02_skb_path
stage03_napi_poll
stage04_ring_dma
stage05_virtio_param
stage06_arm64_migration
stage07_real_queue_model
stage08_async_backend_transport
stage09_multi_queue_scaling
stage10_msix_per_queue_irq
stage11_page_pool_rx
stage12_ethtool_control_plane
stage13_offload_basics
stage14_xdp_basics
```

## 关键证据

- `../../netdev/stage14_xdp_basics/records/`
- `../../netdev/stage14_xdp_basics/records/smoke-20260422_224816/SMOKE_REPORT.md`
- `../../netdev/stage14_xdp_basics/scripts/smoke.sh`
- `../../netdev/stage14_xdp_basics/scripts/xdp_check.sh`
- `../../netdev/stage14_xdp_basics/tools/`

## 已证明

```text
教学型 netdev 主线完成 stage00~stage14
覆盖 net_device、skb、NAPI、ring、多队列、MSI-X、page_pool、ethtool、offload、XDP
stage14 作为 netdev 线性主线收口点
```

## 边界

这是教学型网络驱动主线，不是生产 NIC 驱动。
