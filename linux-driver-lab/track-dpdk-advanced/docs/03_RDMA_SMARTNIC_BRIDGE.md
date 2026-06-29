# RDMA / SmartNIC Bridge

DPDK Advanced 收敛后的自然延伸是 RDMA 和 SmartNIC/DPU。

## DPDK 到 RDMA

| DPDK 概念 | RDMA 对应概念 |
|---|---|
| mbuf / mempool | registered memory / MR |
| RX/TX queue | QP send/recv queue |
| poll mode | CQ polling |
| queue-to-core | QP/CQ affinity |
| VFIO/IOMMU | DMA isolation / memory pinning |

## DPDK 到 SmartNIC/DPU

| DPDK 概念 | SmartNIC/DPU 对应概念 |
|---|---|
| software forwarding | hardware offload |
| ACL / route action | tc flower / rte_flow |
| PMD capability | representor / switchdev / devlink |
| per-rule stats | offload rule counters |

## 推荐后续路线

```text
RDMA Core first
  -> verbs/MR/QP/CQ
  -> RC pingpong
  -> UD/RoCEv2 model
  -> perftest when hardware is available

SmartNIC/DPU later
  -> representor
  -> switchdev
  -> tc flower
  -> OVS offload
```

