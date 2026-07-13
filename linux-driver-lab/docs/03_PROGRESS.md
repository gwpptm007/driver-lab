# 03_PROGRESS

> 当前进度与完成度矩阵。

## 当前完成度总览

| 阶段 | 目录 | 完成度 | 关键里程碑 |
| --- | --- | --- | --- |
| W1 字符设备 | `foundation/day01~day07` | 已完成 | 字符设备驱动闭环 |
| W2 平台/DT/IRQ | `foundation/day08~day14` | 已完成 | `platform_driver` + ftrace |
| W3 baseline/裁剪 | `foundation/day15~day21` | 已完成 | 工程化 baseline + 回归 |
| W4 PCIe | `foundation/day22~day28` | 已完成 | ivshmem-doorbell + MSI |
| W5 DMA/性能 | `foundation/day29~day35` | 已完成 | DMA/mmap/perf/ftrace/stability |
| netdev 主线 | `netdev/stage00~stage14` | 已完成 | stage14 XDP 入口收口 |
| track-real-driver | 4 labs + 1 project | 已完成 | virtio_net 源码深潜 |
| track-virtual-net | 3 labs + 1 project | 已完成 | vhost/kick/notify + L2 转发 |
| track-af-xdp | 4 phases | 已完成 | 全部 PASS |
| track-dpdk | 9 phases | 已完成 | media-gateway-lite PASS_TRAFFIC |
| track-ebpf-observability | 5 phases | 已完成 | 全部 COMPLETED |
| project-linux-network-data-plane | 总收口项目 | 已封版 | network-data-plane-v1 |
| track-dpdk-advanced | 7 phases | 当前环境已收口 | flow pipeline Phase 1-3/5/6 到 `DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE`，Phase 4 保留硬件边界 |
| track-rdma-core | Phase 1~11 | RDMA core + 工程化 + perf tuning + one-sided KV 已收敛到当前环境边界 | Phase 1~8 PASS，RC client/server PASS，performance tuning 到 `PASS_NUMACTL_NODE0_BINDING`，one-sided KV 为 `ONE_SIDED_KV_CURRENT_ENV_COMPLETE` |
| projects/project-network-acceleration-portfolio | portfolio v1 | 已收口 | `PORTFOLIO_V1_READY`，证据索引、面试材料与真实硬件复验清单已建立 |
| projects/project-dpdk-rdma-gateway | Phase 4 baseline | 当前环境已收口 | DPDK producer、SPSC ring、RDMA worker、RXE WRITE/CQE 端到端 PASS |
| track-block-io | P2 支线 | PARKED_PLANNED | block layer / storage I/O 规划保留 |

## 当前推荐入口

```text
track-rdma-core/README.md
track-rdma-core/ROADMAP.md
track-rdma-core/START_HERE.md
track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260713_NUMACTL_NODE0.md
track-rdma-core/project-rdma-one-sided-kv/tests/TEST_RECORD_20260713_PHASE1.md
track-rdma-core/project-rdma-one-sided-kv/tests/TEST_RECORD_20260713_PHASE2_CREDIT_BATCH.md
track-rdma-core/project-rdma-one-sided-kv/tests/TEST_RECORD_20260713_PHASE3_REMOTE_ATOMIC.md
track-rdma-core/project-rdma-one-sided-kv/tests/TEST_RECORD_20260713_PHASE4_CAS_CONTENTION.md
track-rdma-core/project-rdma-one-sided-kv/tests/TEST_RECORD_20260713_PHASE5_DYNAMIC_DIRECTORY.md
track-rdma-core/project-rdma-one-sided-kv/tests/TEST_RECORD_20260713_PHASE6_RKEY_ROTATION.md
track-dpdk-advanced/project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
track-dpdk-advanced/project-dpdk-flow-pipeline/tests/TEST_RECORD_20260713_PHASE1.md
track-dpdk-advanced/project-dpdk-flow-pipeline/tests/TEST_RECORD_20260713_PHASE2_LIFECYCLE.md
track-dpdk-advanced/project-dpdk-flow-pipeline/tests/TEST_RECORD_20260713_PHASE3_WORKER.md
track-dpdk-advanced/project-dpdk-flow-pipeline/tests/TEST_RECORD_20260713_PHASE4_CAPABILITY_BOUNDARY.md
track-dpdk-advanced/project-dpdk-flow-pipeline/tests/TEST_RECORD_20260713_PHASE5_TUNING.md
track-dpdk-advanced/project-dpdk-flow-pipeline/tests/TEST_RECORD_20260713_PHASE6_CLOSEOUT.md
projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE1_CONTRACT.md
projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE2_INGRESS.md
projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE3_RDMA.md
projects/project-dpdk-rdma-gateway/tests/TEST_RECORD_20260713_PHASE4_E2E.md
```

## RDMA 当前状态

RDMA 主线已经从“对象模型学习”推进到“工程化小系统 + 性能调参框架”：

1. Phase 1~8：RDMA core 基础实验全部 PASS。
2. Phase 9：`project-rdma-rc-client-server` 已完成单机 Soft-RoCE、错误边界、CPU affinity/NUMA 记录链路。
3. Phase 10：`project-rdma-performance-tuning` 已完成 SEND latency、batch WR、inline、selective signaling、CQ polling、RTT、CPU affinity、`numactl node0` 绑定路径。
4. Phase 11：`project-rdma-one-sided-kv` 已完成 fixed-slot/batch、remote atomic/CAS、动态 key directory、碰撞拒绝、MR re-register 与 stale rkey 边界的 RXE 闭环。

当前明确边界：

- `192.168.65.135` 只有 `node0`，所以不能声称完成跨 NUMA 性能对比。
- `192.168.65.134` 当前 SSH 登录仍失败，双机 fresh 验证保留为环境阻塞项。
- Soft-RoCE/RXE 数据只能证明模型、流程和脚本闭环，不代表真实 RNIC 性能。

## 下一步建议

优先级建议：

1. 在真实 NIC/RNIC 环境补 `project-dpdk-rdma-gateway` batch WR、selective signaling、RSS/NUMA 性能证据。
2. 有双 client、多 QP、双机或真实 RNIC 环境后，再扩展 one-sided KV 并发与硬件性能证据。
3. 按 `projects/project-network-acceleration-portfolio` 的复验清单补硬件证据。
4. `track-block-io` 保留为 P2 支线，后续与端到端网存路径观测结合。
