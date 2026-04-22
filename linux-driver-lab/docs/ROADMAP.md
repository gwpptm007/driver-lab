# 学习路线图

## W1 字符设备基础

- Day01：骨架
- Day02：ioctl
- Day03：sysfs
- Day04：debugfs
- Day05：waitqueue / workqueue
- Day06：回归测试
- Day07：W1 最终版

## W2 嵌入式通用套路

- Day08：platform_driver + probe/remove + devm 资源管理
- Day09：Device Tree + reg/irq 解析
- Day10：request_irq + /proc/interrupts 中断计数实现
- Day11：bottom-half(workqueue) + 把重活下沉 + 延迟统计
- Day12：regmap 封装寄存器 + debugfs 输出寄存器快照
- Day13：ftrace function_graph 跟踪一次 IRQ 路径 + 截图归档
- Day14：bring-up checklist（1页）：拿寄存器表如何推进联调

## W3 内核裁剪与移植

- defconfig / Kconfig
- rootfs 规划
- 启动链路与回归

## W4 PCIe 基本功作品线

- QEMU PCI 设备选型（ivshmem-doorbell）
- PCI 枚举 / `lspci -vv`
- `pci_driver` 骨架
- BAR / MMIO / 共享内存协议
- 中断 / 用户态工具 / remove / 循环

## W5 DMA + 性能分析作品线

- DMA-capable 后端（推荐 QEMU EDU）
- coherent DMA
- `mmap` 零拷贝
- bench
- perf / ftrace
- 稳定性与最终报告

## 后续扩展


## 第二阶段 netdev 主线（stage00~stage14）

- stage00：bootstrap
- stage01：netdev skeleton
- stage02：skb path
- stage03：napi poll
- stage04：ring dma
- stage05：virtio param
- stage06：arm64 migration
- stage07：real queue model
- stage08：async backend transport
- stage09：multi-queue scaling
- stage10：MSI-X / per-queue IRQ
- stage11：page_pool / RX recycle
- stage12：ethtool / control plane
- stage13：offload basics
- stage14：XDP entry / fast path

## stage14 之后

- 不再继续线性 stage15/stage16
- 改为 `track / lab / project` 组织
- 当前首个推荐专题：`track-real-driver/lab-virtio-net-source-dive/`
