# Evidence Index

## Track principle diagrams

```text
docs/04_ARCHITECTURE_PRINCIPLES.md
```

## Phase 1: mbuf / mempool

```text
lab-dpdk-mbuf-mempool-deep-dive/records/20260629-210538-mbuf-mempool/SUMMARY.md
lab-dpdk-mbuf-mempool-deep-dive/reports/phase1_mbuf_mempool_report.md
lab-dpdk-mbuf-mempool-deep-dive/docs/04_DEEP_LEARNING.md
lab-dpdk-mbuf-mempool-deep-dive/docs/02_TEST_AND_VERIFY.md
```

Key evidence:

```text
PASS_BUILD
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

## Phase 2: RSS / multi-queue

```text
lab-dpdk-rss-multiqueue/records/20260629-211820-rss-multiqueue/SUMMARY.md
lab-dpdk-rss-multiqueue/reports/phase2_rss_multiqueue_report.md
lab-dpdk-rss-multiqueue/docs/04_DEEP_LEARNING.md
lab-dpdk-rss-multiqueue/docs/02_TEST_AND_VERIFY.md
```

Key evidence:

```text
max_rx_queues=1
reta_size=0
rss_offloads=0x0
BLOCKED_PCAP_RSS
```

## Phase 3: NUMA / burst tuning

```text
lab-dpdk-numa-burst-tuning/records/20260629-212218-numa-burst/SUMMARY.md
lab-dpdk-numa-burst-tuning/records/20260629-212218-numa-burst/MATRIX.csv
lab-dpdk-numa-burst-tuning/reports/phase3_numa_burst_report.md
lab-dpdk-numa-burst-tuning/docs/04_DEEP_LEARNING.md
lab-dpdk-numa-burst-tuning/docs/02_TEST_AND_VERIFY.md
```

Key evidence:

```text
PASS_BURST_MATRIX
PASS_CACHE_MATRIX
PASS_CPU_RECORD
```

## Phase 4: VFIO / IOMMU boundary

```text
lab-dpdk-vfio-iommu-boundary/records/20260629-212638-vfio-iommu/SUMMARY.md
lab-dpdk-vfio-iommu-boundary/reports/phase4_vfio_iommu_report.md
lab-dpdk-vfio-iommu-boundary/docs/04_DEEP_LEARNING.md
lab-dpdk-vfio-iommu-boundary/docs/02_TEST_AND_VERIFY.md
```

Key evidence:

```text
iommu_group_entries=0
vfio_module_loaded=no
uio_module_loaded=no
```

## Phase 5: L3 forwarder lite

```text
project-dpdk-l3-forwarder-lite/records/20260629-213104-l3-forwarder/SUMMARY.md
project-dpdk-l3-forwarder-lite/reports/phase5_l3_forwarder_lite_report.md
project-dpdk-l3-forwarder-lite/docs/04_DEEP_LEARNING.md
project-dpdk-l3-forwarder-lite/docs/02_TEST_AND_VERIFY.md
```

Key evidence:

```text
PASS_L3_FORWARD
PASS_ACL_DROP
PASS_PER_RULE_STATS
```

## Phase 6: advanced summary

```text
project-dpdk-advanced-summary/docs/04_DEEP_LEARNING.md
project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md
project-dpdk-advanced-summary/reports/TUNING_CHECKLIST.md
project-dpdk-advanced-summary/reports/INTERVIEW_NOTES.md
project-dpdk-advanced-summary/reports/RESUME_MATERIAL.md
project-dpdk-advanced-summary/reports/RDMA_TRANSITION_NOTES.md
```
