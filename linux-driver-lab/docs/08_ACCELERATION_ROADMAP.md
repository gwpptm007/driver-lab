# Acceleration Roadmap

> network-data-plane-v1 之后的高性能网络加速路线图。

## 总体方向

当前优先方向从“泛 Linux I/O 完整性”调整为“高性能网络 / RDMA / SmartNIC / DPU”：

```text
network data plane v1
  -> DPDK Advanced
  -> RDMA Core
  -> SmartNIC / DPU
  -> acceleration portfolio
```

`track-block-io` 暂时保留为 P2 支线，用于后续补齐 storage I/O 与 blk-mq 能力，但不抢占当前主线。

## 为什么这样排

`project-linux-network-data-plane` 已经完成：

```text
netdev
real driver
virtual net
DPDK basic fastpath
AF_XDP
eBPF observability
```

下一步如果目标是 RDMA / SmartNIC / DPU，最自然的承接不是 block layer，而是把 DPDK 从“能跑通 fastpath”推进到“理解真实数据中心网络加速的性能与部署边界”。

## 主线分层

| Order | Track | 重点 | 产出 |
|------|-------|------|------|
| 1 | `track-dpdk-advanced` | mbuf/mempool、RSS、多队列、NUMA、VFIO/IOMMU、调优方法 | DPDK 进阶报告与调优 checklist |
| 2 | `track-rdma` | verbs、QP/CQ/MR、RC/UD、RoCEv2、perftest | RDMA core lab 与 DPDK/RDMA 对比 |
| 3 | `track-smartnic-dpu` | representor、switchdev、devlink、tc flower、OVS offload | SmartNIC/DPU 架构与 offload map |
| 4 | `project-network-acceleration-portfolio` | DPDK/RDMA/eBPF/tc/SmartNIC 横向整合 | 最终作品集、面试材料、简历材料 |

## 当前立即启动项

当前只落地第一条：

```text
linux-driver-lab/track-dpdk-advanced/
```

RDMA 和 SmartNIC/DPU 先进入路线图，不创建大量空目录。等 DPDK Advanced 收口后再启动下一条 track。

## 边界

准确表述：

- 当前是高性能网络加速路线规划。
- DPDK Advanced 是下一条可执行主线。
- RDMA / SmartNIC / DPU 是后续主线，不是当前已完成内容。

不要夸大：

- 不说已经具备 RDMA 生产经验。
- 不说已经完成 DPU offload。
- 不说当前 DPDK 已覆盖真实生产多队列/RSS/NUMA 全部调优。

