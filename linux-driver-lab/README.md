# linux-driver-lab

Linux 驱动学习实验主目录。从 foundation（day01~day35）到 netdev（stage00~stage14），再到 track 专题研究，形成完整的驱动学习路径。

## 目录结构

```
linux-driver-lab/
├── docs/           项目文档（专家评审、进度、架构）
├── foundation/      ⭐ 第一阶段：day01~day35（W1~W5）
├── netdev/         ⭐ 第二阶段：stage00~stage14
├── track-real-driver/   第三阶段：真实驱动源码专题
├── track-virtual-net/   虚拟化网络：tap/bridge/vhost
├── track-af-xdp/        AF_XDP 快速路径
├── track-dpdk/          DPDK 用户态网络
├── track-dpdk-advanced/ DPDK 进阶：多队列/RSS/NUMA/VFIO/调优
├── track-rdma-core/     RDMA core：verbs/MR/QP/RC、性能调优与 one-sided KV
├── track-ebpf-observability/  eBPF 可观测性
├── projects/        跨 track 综合项目与作品集
└── track-block-io/      P2 保留支线：block layer / storage I/O
```

## 快速导航

| 阶段 | 入口 | 说明 |
|------|------|------|
| 总览 | `docs/05_START_HERE.md` | 快速入门与推荐阅读顺序 |
| 项目状态 | `docs/01_PROGRAMS.md` / `docs/03_PROGRESS.md` | track 定位与完成度矩阵 |
| foundation | `foundation/README.md` | W1~W5 完整学习路径 |
| netdev | `netdev/README.md` | stage00~stage14 网络驱动主线 |
| track-real-driver | `track-real-driver/README.md` | 真实驱动源码与 patch 线 |
| track-virtual-net | `track-virtual-net/README.md` | vhost/kick/notify 机制 |
| track-dpdk | `track-dpdk/README.md` | DPDK 用户态网络 fastpath |
| track-dpdk-advanced | `track-dpdk-advanced/README.md` | DPDK 进阶：mbuf/mempool、RSS、多队列、NUMA、VFIO/IOMMU |
| track-rdma-core | `track-rdma-core/README.md` | Phase 1-11 当前环境收口，one-sided KV Phase 1-6 完成 |
| track-af-xdp | `track-af-xdp/README.md` | Linux 原生 XDP + AF_XDP socket（四阶段全部 PASS） |
| track-ebpf-observability | `track-ebpf-observability/README.md` | 网络路径观测与定位 |
| network-acceleration-portfolio | `projects/project-network-acceleration-portfolio/README.md` | 已验证证据、面试材料与真实硬件复验路线 |
| dpdk-rdma-gateway | `projects/project-dpdk-rdma-gateway/README.md` | DPDK ingress + RXE RDMA egress capstone，Phase 1-4 当前环境完成 |
| track-block-io | `track-block-io/README.md` | P2 保留支线：bio / request / blk-mq / fio / observability |

详细说明 → `docs/01_PROGRAMS.md`
