# 04_ARCHITECTURE

> 架构分层与完成度矩阵

## 项目架构分层

```
linux-driver-lab/
├── foundation/          第一阶段：W1~W5 基础实验主线
├── netdev/              第二阶段：stage00~stage14 网络驱动主线
├── track-real-driver/   第三阶段：真实驱动源码研究
├── track-virtual-net/   虚拟化网络：tap/bridge/vhost 协同
├── track-af-xdp/        AF_XDP 快速路径
├── track-dpdk/          DPDK 用户态网络
└── track-ebpf-observability/  eBPF 可观测性
```

---

## 各阶段定位

| 阶段 | 核心能力 | 关键产出 |
|------|---------|---------|
| foundation | 字符设备 / 平台驱动 / PCIe / DMA | 可交付的 day01~day35 作品线 |
| netdev | net_device / skb / NAPI / ring / XDP | stage00~stage14 教学驱动 |
| track-real-driver | 源码深潜 / patch 实验 / 真实驱动理解 | 多个 lab + project |
| track-virtual-net | vhost / tap / bridge / L2 转发 | 集成测试报告 |

---

## 完成度矩阵

| 模块 | 覆盖 | 状态 | 说明 |
|------|------|------|------|
| 字符设备驱动 | day01~day07 | ✅ | open/read/write/ioctl/mmap |
| 平台驱动/DT/IRQ | day08~day14 | ✅ | platform_driver + ftrace |
| Baseline工程化 | day15~day21 | ✅ | defconfig + rootfs + 回归 |
| PCIe驱动 | day22~day28 | ✅ | BAR/MMIO + MSI + ivshmem |
| DMA/性能 | day29~day35 | ✅ | dma_alloc_coherent + mmap + perf |
| netdev骨架 | stage00~stage01 | ✅ | alloc_etherdev + register_netdev |
| skb路径 | stage02 | ✅ | alloc_skb + skb_put/pull |
| NAPI轮询 | stage03 | ✅ | napi_poll + budget |
| ring/DMA | stage04 | ✅ | descriptor ring + streaming DMA |
| virtio对照 | stage05 | ✅ | virtio-net 源码对照 |
| ARM64迁移 | stage06 | ✅ | 跨平台 build/run |
| 多队列 | stage07~stage09 | ✅ | queue lifecycle + async backend |
| MSI-X | stage10 | ✅ | per-queue IRQ |
| page_pool | stage11 | ✅ | RX page recycling |
| ethtool | stage12 | ✅ | control plane |
| offload | stage13 | ✅ | checksum/GRO/GSO |
| XDP入口 | stage14 | ✅ | XDP_PASS/DROP/REDIRECT |
| 真实驱动源码 | track-real-driver | ✅ | virtio_net / e1000e 源码深潜 |
| 虚拟化网络 | track-virtual-net | ✅ | tap/bridge/vhost/L2转发 |