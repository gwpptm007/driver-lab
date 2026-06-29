# project-dpdk-advanced-summary

DPDK Advanced 的阶段性收口项目。

## 状态

```text
COMPLETED
```

## 产物

- `docs/04_DEEP_LEARNING.md`
- `reports/DPDK_ADVANCED_FINAL_REPORT.md`
- `reports/EVIDENCE_INDEX.md`
- `reports/TUNING_CHECKLIST.md`
- `reports/INTERVIEW_NOTES.md`
- `reports/RESUME_MATERIAL.md`
- `reports/RDMA_TRANSITION_NOTES.md`

## 总体结论

这个 track 已经从基础 DPDK fastpath 继续推进到：

```text
mbuf/mempool metadata
queue/RSS boundary
burst/cache/NUMA tuning method
VFIO/IOMMU deployment boundary
L3 forwarding / ACL / per-rule stats
```

当前测试环境是 VMware + pcap PMD / net_null PMD，因此所有结论都按 evidence 分级：

- pcap PMD 能证明软件数据面逻辑。
- VMware/vmxnet3 能记录真实部署边界。
- RSS/VFIO/真实 NIC 线速不夸大，作为后续硬件环境验证项。
