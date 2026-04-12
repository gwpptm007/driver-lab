# stage04_ring_dma

## 阶段定位

这是 netdev 主线里最接近“真实驱动心智模型”的阶段：

- 不再只停留在 `skb`/`NAPI` 教学闭环
- 开始引入 **descriptor / ring / ownership / completion**
- 引入 **streaming DMA map/unmap** 的教学型实现
- 把 **RX replenishment** 当成独立核心主题做透

## 本阶段不追求什么

- 不追求真实物理网卡吞吐
- 不追求复杂 offload / 多队列
- 不追求一下子对齐 `virtio-net` 全部能力

本阶段追求的是：**先把 ring + DMA + refill 这套心智模型建立起来**。

## 当前实现摘要

当前 `driver/netdev_stage04.c` 已经落下：

- 一个教学型 `net_device` 驱动 `nds4`
- TX ring：用 descriptor 模拟“设备读取待发 buffer”
- RX ring：预投递 buffer，设备写入后交还给 CPU
- `dma_map_single()` / `dma_unmap_single()` 教学型路径
- NAPI poll drain RX done descriptors
- 每处理完一个 RX descriptor，立即做 **replenishment**
- `debugfs` 统计与 ring 状态导出

## 建议先看

1. `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
2. `docs/02_RING_AND_DMA_MODEL.md`
3. `docs/03_RX_REPLENISHMENT.md`
4. `driver/netdev_stage04.c`
5. `scripts/smoke.sh`

## 常用命令

```bash
cd linux-driver-lab/netdev/stage04_ring_dma
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
