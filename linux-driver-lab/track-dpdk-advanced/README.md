# track-dpdk-advanced

DPDK 进阶主线：承接已完成的 `track-dpdk`，把能力从“fastpath 能跑通”推进到“能解释数据面性能、队列、内存、部署边界和小型 L3 转发项目”。

## 状态

```text
COMPLETED_WITH_BOUNDARIES
```

当前 VMware 测试机已完成可复现实验和记录归档。RSS 多队列和 VFIO/IOMMU 真实硬件能力受环境限制，已作为 boundary evidence 收敛，不做夸大。

## 阶段结果

| Phase | 目录 | 状态 | 目标 |
|---|---|---|---|
| Phase 1 | `lab-dpdk-mbuf-mempool-deep-dive` | `PASS_PCAP_METADATA` | 理解 mbuf、mempool、metadata、offload flags |
| Phase 2 | `lab-dpdk-rss-multiqueue` | `BLOCKED_PCAP_RSS` | 理解 RSS、多 RX/TX queue、queue-to-core mapping |
| Phase 3 | `lab-dpdk-numa-burst-tuning` | `PASS_TUNING_METHOD` | 建立 burst/cache/NUMA 对比方法 |
| Phase 4 | `lab-dpdk-vfio-iommu-boundary` | `PASS_VFIO_IOMMU_BOUNDARY` | 复盘 UIO/VFIO/IOMMU/MSI-X 环境边界 |
| Phase 5 | `project-dpdk-l3-forwarder-lite` | `PASS_L3_FORWARDER_LITE` | 实现 L3 forwarding / ACL / per-rule stats 小项目 |
| Phase 6 | `project-dpdk-advanced-summary` | `PASS_ADVANCED_REPORT` | 汇总报告、证据索引、面试材料、RDMA 过渡 |

## 推荐入口

```text
project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md
project-dpdk-l3-forwarder-lite/README.md
```

## 能力结构

```text
mbuf / mempool
  -> queue / RSS / multi-core boundary
  -> NUMA / cache / burst tuning method
  -> VFIO / IOMMU / MSI-X deployment boundary
  -> L3 forwarding / ACL / stats
  -> DPDK advanced report
```

## 当前边界

准确表述：

- pcap PMD 能证明软件数据面逻辑。
- net_null PMD 能作为 TX sink 验证转发路径和统计。
- 当前 pcap PMD 不能证明真实 RSS 多队列。
- 当前 VMware 启动参数没有 IOMMU，不能证明真实 VFIO/IOMMU 隔离。
- 当前结果不等于真实 100G NIC 线速调优。

不要夸大：

- 不说完成生产级 DPDK 网关。
- 不说完成真实硬件 RSS/VFIO 全量验证。
- 不说当前 VMware 环境覆盖所有部署形态。

