# track-block-io

## 知识基础文档

本支线启动前，先阅读 [docs/fundamentals/README.md](docs/fundamentals/README.md)。该目录补齐 block stack、bio、现代 blk-mq、ramdisk 生命周期、完成/持久性、性能、观测和安全实验的基础；它只提供 Markdown 设计材料，不改变本 track 仍为 PARKED_PLANNED 的状态。

> P2 保留支线：Linux block layer / storage I/O 驱动与高性能 I/O。

当前状态：`PARKED_PLANNED`。

说明：本 track 先保留规划，不作为当前主线推进。当前优先方向调整为高性能网络加速：

```text
track-dpdk-advanced
  -> track-rdma
  -> track-smartnic-dpu
```

## 一句话定位

承接已经封版的 `project-linux-network-data-plane`，把已有的 queue、DMA、completion、observability 能力从网络数据面迁移到存储 I/O：

```text
network data plane:
  skb / NAPI / ring / XDP / PMD / AF_XDP

block I/O:
  bio / request / request_queue / gendisk / blk-mq / completion
```

本 track 的目标不是直接写生产级磁盘驱动，而是在需要补齐 storage I/O 时，形成一条可复现、可解释、可交付的 block I/O 学习与作品线。

## 阶段路线

| Phase | 目录 | 状态 | 目标 |
|------|------|------|------|
| Phase 1 | `lab-simple-ramdisk` | PLANNED | 注册最小 block device，完成 read/write/mkfs/mount/fio smoke |
| Phase 2 | `lab-bio-request-path` | PLANNED | 梳理 bio -> request -> queue -> completion 数据路径 |
| Phase 3 | `lab-blk-mq-ramdisk` | PLANNED | 从简单 request queue 推进到 blk-mq 模型 |
| Phase 4 | `lab-virtio-blk-source-dive` | PLANNED | 阅读 virtio-blk，并映射到自写 ramdisk 模型 |
| Phase 5 | `lab-fio-latency-observe` | PLANNED | 用 fio/iostat/blktrace/bpftrace 建立延迟与吞吐观测 |
| Phase 6 | `project-block-io-track-summary` | PLANNED | 汇总报告、证据索引、面试材料和后续 backlog |

## 推荐推进顺序

```text
lab-simple-ramdisk
  -> lab-bio-request-path
  -> lab-blk-mq-ramdisk
  -> lab-virtio-blk-source-dive
  -> lab-fio-latency-observe
  -> project-block-io-track-summary
```

## 第一阶段验收标准

`lab-simple-ramdisk` 先做最小但完整的 block device 闭环：

```text
PASS_REGISTER      /dev/labram0 或等价 block device 出现
PASS_READ_WRITE    dd 读写成功
PASS_MKFS          mkfs.ext4 成功
PASS_MOUNT         mount 后可创建/读取文件
PASS_FIO_SMOKE     fio small workload 有吞吐和延迟输出
PASS_CLEANUP       rmmod 后设备和挂载清理干净
```

## 当前边界

准确表述：

- 这是 block I/O 支线规划，不是当前主线，也不是已完成项目。
- 第一阶段目标是教学型 ramdisk block driver，不是真实磁盘或 NVMe 驱动。
- 性能观测先以 fio smoke 和基础 latency/throughput 为准，不做生产级压测。

不要夸大：

- 不说生产级 storage driver。
- 不说完成真实 NVMe 性能优化。
- 不说覆盖完整 filesystem / page cache / writeback 语义。
