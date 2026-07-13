# track-rdma-core

这条 track 用来把 RDMA 从“听过很多名词”推进到“能解释、能验证、能落证据”的状态。

当前已经完成三层内容：

1. RDMA core 基础：verbs 对象、MR/QP/CQ、RC、one-sided、UD/RoCEv2。
2. 工程化项目：独立 `rdma-rc-server` / `rdma-rc-client`、TCP 控制面、RC 数据面、错误边界。
3. 性能调参项目：SEND latency、batch WR、inline、selective signaling、CQ polling、RTT、CPU affinity、`numactl node0` 绑定路径。

## 第一次进入先读这里

不要直接从某个 `lab-*` 的 `main.c` 开始。先打开 [docs/fundamentals/README.md](docs/fundamentals/README.md)，按“系统分层 -> verbs 对象 -> MR/key -> QP 状态机 -> WR/CQE -> transport -> one-sided -> 性能与故障”的顺序建立完整模型。

这套前置知识包含 13 个主题，既覆盖入门对象和数据路径，也覆盖 RoCEv2 拥塞、Atomic 一致性、rkey 轮换、NUMA、故障恢复和分层排障。每章最后都映射到本仓库的实验或工程项目。

文档阶段状态：`RDMA_FUNDAMENTALS_COMPLETE`。

## 当前进度

| 阶段 | 目录 | 目标 | 状态 |
| --- | --- | --- | --- |
| Phase 0 | `docs/fundamentals/` | RDMA 完整知识底座、原理图、项目映射与排障卡 | PASS 文档审计 |
| Phase 1 | `lab-rdma-env-capability` | RDMA 工具、设备、内核模块、Soft-RoCE 边界采集 | PASS |
| Phase 2 | `lab-rdma-verbs-object-lifecycle` | device/context/PD/MR/CQ/QP 生命周期 | PASS |
| Phase 3 | `lab-rdma-memory-region-deep-dive` | MR、lkey/rkey、access flags | PASS |
| Phase 4 | `lab-rdma-qp-state-machine` | RC QP 状态迁移 | PASS |
| Phase 5 | `lab-rdma-rc-pingpong` | RC send/recv 与 CQ completion | PASS |
| Phase 6 | `lab-rdma-one-sided-read-write` | RDMA READ/WRITE one-sided 语义 | PASS |
| Phase 7 | `lab-rdma-ud-rocev2-model` | UD/RoCEv2 报文路径与封装模型 | PASS |
| Phase 8 | `project-rdma-core-summary` | 总结报告、面试材料、DPDK 到 RDMA 对照 | PASS |
| Phase 9 | `project-rdma-rc-client-server` | 独立 server/client、控制面、数据面、故障边界 | PASS 当前环境边界 |
| Phase 10 | `project-rdma-performance-tuning` | latency、batch、inline、selective、polling、RTT、affinity/NUMA | `PASS_NUMACTL_NODE0_BINDING` |
| Phase 11 | `project-rdma-one-sided-kv` | batch、atomic/CAS、动态目录、rkey 轮换 | `ONE_SIDED_KV_CURRENT_ENV_COMPLETE` |

## 学习重点

- RDMA 不是“更快的 socket”，而是把数据路径从 syscall + kernel protocol stack 转为 NIC DMA + userspace queue pair。
- verbs 程序的核心不是 API 背诵，而是对象生命周期：device -> context -> PD -> MR -> CQ -> QP -> WQE -> CQE。
- 没有真实 RNIC 时也要诚实收敛：Soft-RoCE 可以验证模型、状态机、控制面和数据面流程，但不能代表硬件性能。
- 性能结论必须带上下文：CPU affinity、NUMA、CQ polling、signaling、inline、batch size 都会影响结果。

## 入口

建议阅读顺序：

```text
docs/fundamentals/README.md
docs/fundamentals/00_15_MINUTE_MENTAL_MODEL.md
docs/fundamentals/01_HARDWARE_KERNEL_USERSPACE_STACK.md
docs/fundamentals/02_VERBS_OBJECTS_LIFECYCLE.md
docs/fundamentals/03_MEMORY_REGISTRATION_DMA_KEYS.md
docs/fundamentals/04_QP_STATE_MACHINE_CONTROL_PLANE.md
docs/fundamentals/05_WR_WQE_CQE_DATA_PATH.md
docs/fundamentals/06_TRANSPORTS_ROCE_NETWORK.md
docs/fundamentals/07_ONE_SIDED_ATOMIC_CONSISTENCY.md
docs/fundamentals/08_PERFORMANCE_TUNING_NUMA.md
docs/fundamentals/09_RELIABILITY_SECURITY_FAILURES.md
docs/fundamentals/10_PROJECT_KNOWLEDGE_MAP.md
docs/fundamentals/11_DEBUGGING_PLAYBOOK.md
docs/fundamentals/12_RECALL_CARDS.md
docs/01_TRACK_OVERVIEW.md
docs/02_RDMA_CORE_MODEL.md
docs/04_DPDK_TO_RDMA_BRIDGE.md
ROADMAP.md
START_HERE.md
project-rdma-rc-client-server/README.md
project-rdma-performance-tuning/README.md
```

关键证据入口：

```text
project-rdma-core-summary/EVIDENCE_INDEX.md
project-rdma-rc-client-server/tests/TEST_RECORD_20260712_AFFINITY.md
project-rdma-performance-tuning/tests/TEST_RECORD_20260713_NUMACTL_NODE0.md
tests/TEST_RECORD_20260714_RDMA_FUNDAMENTALS.md
```

## 当前边界

- `192.168.65.135` 上 Soft-RoCE/RXE 主线已跑通。
- `numactl` 已补装，`PERF_SERVER_NUMA_NODE=0` / `PERF_CLIENT_NUMA_NODE=0` 同节点绑定路径 PASS。
- 135 当前只有 `node0`，所以不能声称完成跨 NUMA 性能对比。
- `192.168.65.134` 当前 SSH 登录仍失败，双机 fresh perf 验证保留为环境阻塞项。
- 所有 RXE 性能数据只作为学习与相对行为观察，不包装成真实 RNIC 性能。

## 下一步

如果继续 RDMA：

1. 拿到 134 可登录账号后补 `134 -> 135` 双机 fresh perf。
2. 在多 NUMA node 环境补同节点 / 跨节点对比。
3. 有真实 RNIC 后重跑 latency、batch、inline、selective signaling、CQ polling。

如果继续主线：

1. `projects/project-network-acceleration-portfolio` 已完成 `PORTFOLIO_V1_READY`，用于汇总 DPDK/RDMA/eBPF/tc/SmartNIC/DPU 的模型、证据和边界。
2. `project-rdma-one-sided-kv` Phase 1-6 已完成 `ONE_SIDED_KV_CURRENT_ENV_COMPLETE`；真实双 client、多 QP、重连和 RNIC 性能保留为硬件/拓扑扩展。
