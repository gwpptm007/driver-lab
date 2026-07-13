# Acceleration Roadmap

> `network-data-plane-v1` 之后的高性能网络 / RDMA / SmartNIC / DPU 路线图。

## 总体方向

当前主线是高性能网络加速，不再优先扩展泛 Linux I/O：

```text
network data plane v1
  -> DPDK Advanced
  -> RDMA Core
  -> RDMA RC client/server
  -> RDMA performance tuning
  -> network acceleration portfolio
  -> SmartNIC / DPU map
```

`track-block-io` 保留为 P2 支线，用来后续补齐 storage I/O 与 blk-mq 能力，但不抢占当前主线。

## 当前完成状态

| Order | Track | 当前状态 | 产出 |
| --- | --- | --- | --- |
| 1 | `track-dpdk-advanced` | 已收敛 | DPDK 进阶报告、调优 checklist、boundary evidence |
| 2 | `track-rdma-core` Phase 1~8 | PASS | verbs/MR/QP/CQ/RC/UD/RoCEv2 基础模型 |
| 3 | `project-rdma-rc-client-server` | PASS 当前环境边界 | TCP 控制面、RC 数据面、错误边界、CPU affinity/NUMA 记录 |
| 4 | `project-rdma-performance-tuning` | `PASS_NUMACTL_NODE0_BINDING` | SEND latency、batch WR、inline、selective signaling、CQ polling、RTT、`numactl node0` 验证 |
| 5 | `projects/project-network-acceleration-portfolio` | `PORTFOLIO_V1_READY` | DPDK/RDMA/eBPF/tc/SmartNIC 横向整合、证据索引与复验清单 |
| 6 | `track-rdma-core/project-rdma-one-sided-kv` | `ONE_SIDED_KV_CURRENT_ENV_COMPLETE` | batch、atomic/CAS、动态目录、rkey 轮换 |
| 7 | `track-dpdk-advanced/project-dpdk-flow-pipeline` | `DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE` | `rte_hash`、规则生命周期、双 worker、调优矩阵、错误边界 |
| 8 | `projects/project-dpdk-rdma-gateway` | `DPDK_RDMA_GATEWAY_CURRENT_ENV_COMPLETE` | DPDK producer、SPSC ring、RDMA worker、RXE WRITE/CQE 端到端路径 |

## 当前立即执行项

当前主线按以下顺序推进：

1. gateway 基线已收口；在真实 NIC/RNIC 环境补 batch WR、selective signaling、RSS/NUMA 性能矩阵。
2. flow pipeline Phase 4 保持硬件分支：在支持多队列/RSS 的真实 PMD 上补 RETA 和 `rte_flow` hardware rule。
3. 环境补验分支：拿到 `192.168.65.134` 可登录账号、真实 NIC 或多 NUMA 节点后，按 portfolio 的清单补硬件证据。

RDMA one-sided KV、DPDK flow pipeline 与 DPDK-RDMA gateway 基线均已在当前环境收口。gateway Phase 1-4 已完成 contract、pcap ingress、RXE backend 与 integrated worker；真实硬件性能仍保留 capability boundary。

## 边界

准确表述：

- 当前具备高性能网络加速路线的系统性学习材料。
- DPDK Advanced 已收敛，但 RSS/VFIO/真实多队列硬件能力仍按 boundary evidence 表述。
- RDMA core 已完成对象模型、RC/UD/RoCEv2、工程化 server/client 和性能调参框架。
- `numactl` 已在 135 上补装，`node0` 绑定路径 PASS。
- 135 只有 `node0`，所以跨 NUMA 性能对比尚未完成。
- SmartNIC/DPU 是后续主线，不是当前已完成内容。

不要夸大：

- 不说已经具备 RDMA 生产经验。
- 不说已经完成 DPU offload。
- 不说当前 DPDK 已覆盖真实生产多队列/RSS/NUMA 全部调优。
- 不把 Soft-RoCE/RXE 数据包装成真实 RNIC 性能。

## 下一步建议

```text
projects/
  project-network-acceleration-portfolio/  # PORTFOLIO_V1_READY
  project-dpdk-rdma-gateway/               # 综合 capstone
track-rdma-core/
  project-rdma-one-sided-kv/               # CURRENT_ENV_COMPLETE
track-dpdk-advanced/
  project-dpdk-flow-pipeline/              # CURRENT_ENV_COMPLETE
```

作品集、RDMA one-sided KV 与 DPDK flow pipeline 当前环境阶段已经完成；下一条主线是 DPDK 前端与 RDMA 后端的综合 capstone，硬件 capability 复验作为并行分支保留。
