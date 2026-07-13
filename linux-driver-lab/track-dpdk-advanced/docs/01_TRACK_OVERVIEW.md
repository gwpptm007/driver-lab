# 01_TRACK_OVERVIEW

`track-dpdk-advanced` 是 DPDK 进阶训练主线。它承接基础 DPDK track，不再重复 hugepage、EAL、testpmd 的入门闭环，而是继续补齐数据面工程能力。

## 目标能力

- 理解 mbuf / mempool 的 packet memory model。
- 判断 RSS、多队列、queue-to-core 是否真的被当前 PMD 支持。
- 建立 burst size、mempool cache、CPU/NUMA 的调优实验方法。
- 讲清 UIO、VFIO、IOMMU、MSI-X、vmxnet3 的部署边界。
- 实现一个小型 L3 forwarding / ACL / per-rule stats 数据面项目。
- 实现 `rte_hash` flow pipeline、规则生命周期和双 worker 软件模型。

## 最终状态

```text
COMPLETED_WITH_BOUNDARIES
```

## 阶段结果

| Phase | 目录 | 状态 |
|---|---|---|
| 1 | `lab-dpdk-mbuf-mempool-deep-dive` | `PASS_PCAP_METADATA` |
| 2 | `lab-dpdk-rss-multiqueue` | `BLOCKED_PCAP_RSS` |
| 3 | `lab-dpdk-numa-burst-tuning` | `PASS_TUNING_METHOD` |
| 4 | `lab-dpdk-vfio-iommu-boundary` | `PASS_VFIO_IOMMU_BOUNDARY` |
| 5 | `project-dpdk-l3-forwarder-lite` | `PASS_L3_FORWARDER_LITE` |
| 6 | `project-dpdk-advanced-summary` | `PASS_ADVANCED_REPORT` |
| 7 | `project-dpdk-flow-pipeline` | `DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE` |

## DPDK advanced model

```text
PMD / ethdev port
  -> RX queue
  -> rte_eth_rx_burst()
  -> rte_mbuf
  -> parse / classify / inspect / tune
  -> rte_eth_tx_burst() or rte_pktmbuf_free()
```

当前项目按这条路径拆解：

- Phase 1 看 mbuf / mempool。
- Phase 2 看 queue / RSS capability。
- Phase 3 看 burst / cache / NUMA 变量控制。
- Phase 4 看 PMD binding、VFIO / IOMMU / MSI-X 部署边界。
- Phase 5 把 parse / classify / action / TX 做成 L3 forwarder lite。
- Phase 7 深入 exact flow、动态规则、ring/worker、profiling 和硬件能力边界。

## 为什么保留 boundary

当前测试环境是 VMware，并主要使用 pcap PMD / net_null PMD 做可复现验证。

因此：

- 软件数据面逻辑可以验证。
- pcap PMD 的 RSS 能力限制必须记录为 boundary。
- 没有 IOMMU group 时，VFIO 只能做 prerequisites checklist，不能宣称真实验证完成。
- 当前结果不等于真实 NIC 线速调优。
