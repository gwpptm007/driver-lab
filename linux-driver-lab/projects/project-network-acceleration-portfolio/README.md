# project-network-acceleration-portfolio

## 知识基础文档

在阅读既有作品集材料前，建议先从 [docs/fundamentals/README.md](docs/fundamentals/README.md) 开始。该目录从统一数据路径、技术选型、队列内存与完成语义出发，延伸到 NUMA 性能、可观测性、SmartNIC/DPU、可靠性、证据等级和项目演示；它只补 Markdown 基础，不新增性能或 offload 结论。

这个项目是前面网络加速学习线的作品集收口，不新增新的 datapath 代码，重点是把已经验证过的材料组织成一套能展示、能复盘、能面试讲清楚的成果。

覆盖范围：

- Linux kernel netdev / NAPI / ring / XDP 基础
- real driver source dive 与低风险 patch 思路
- virtual net / vhost / tap / bridge
- DPDK userspace fastpath 与调优边界
- AF_XDP / XDP 原生 fastpath
- eBPF observability
- RDMA core、RC client/server、performance tuning
- SmartNIC / DPU 后续地图

## 当前状态

```text
PORTFOLIO_V1_READY
```

第一版已经具备三件事：技术主线地图、已验证证据索引、后续硬件补证清单。它不声称完成新的性能验证。

## 阅读顺序

```text
docs/01_PORTFOLIO_MAP.md
docs/02_DPDK_RDMA_COMPARISON.md
docs/03_PERFORMANCE_TUNING_BOUNDARIES.md
docs/04_INTERVIEW_STORIES.md
docs/05_RESUME_MATERIAL.md
docs/06_SMARTNIC_DPU_MAP.md
docs/07_PORTFOLIO_DEMO_RUNBOOK.md
tests/EVIDENCE_INDEX.md
tests/REVALIDATION_CHECKLIST.md
```

## 验收口径

这套作品集要做到：

- 能从 Linux 内核网络驱动讲到 DPDK / AF_XDP / RDMA。
- 能说明每条路径解决什么问题、绕开什么成本、引入什么约束。
- 能指出哪些内容已经有实验 PASS，哪些只是 boundary evidence。
- 能诚实说明 Soft-RoCE、pcap PMD、veth、虚拟机环境和真实硬件之间的差异。

不做夸大：

- 不把 RXE 数据包装成真实 RNIC 性能。
- 不把 pcap PMD 结果包装成真实 NIC forwarding 性能。
- 不声称已经完成 SmartNIC/DPU offload。
- 不声称覆盖生产级 RSS/VFIO/NUMA 全量调优。

## 当前下一步

作品集本身已可作为展示入口。演示或复盘先按 `docs/07_PORTFOLIO_DEMO_RUNBOOK.md` 串联结论、证据和边界；后续按 `tests/REVALIDATION_CHECKLIST.md` 在真实 NIC、双机和多 NUMA 环境中补证。SmartNIC/DPU 的实施顺序、观测点和验收条件见 `docs/06_SMARTNIC_DPU_MAP.md`。
