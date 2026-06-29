# RDMA Transition Notes

DPDK Advanced 收口后，下一条自然主线是 RDMA。

## 从 DPDK 到 RDMA 的概念桥

| DPDK | RDMA |
|---|---|
| mbuf / mempool | MR / registered memory |
| RX/TX queue | QP send/recv queue |
| completion / stats | CQ / WC |
| poll mode driver | userspace verbs polling |
| RSS / queue mapping | QP / CQ / core affinity |
| VFIO/IOMMU boundary | RNIC DMA isolation / IOMMU / pinning |

## 推荐下一阶段

```text
track-rdma-core
  -> lab-rdma-env-and-verbs
  -> lab-rdma-mr-qp-cq
  -> lab-rdma-rc-pingpong
  -> lab-rdma-ud-and-rocev2
  -> project-rdma-latency-throughput-report
```

## 延续原则

- 继续保留 evidence，而不是只写概念。
- 没有 RNIC 时先做环境探测和 verbs 模型，不伪造 RoCE/RDMA 性能。
- 一旦有硬件，再补真实 perftest、QP/CQ/MR 记录。

