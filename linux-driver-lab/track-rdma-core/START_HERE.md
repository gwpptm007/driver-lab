# START_HERE

## 先读什么

RDMA 这条线先读原理，再跑命令：

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
README.md
```

重点先搞清楚：

- `rdma-core`、`libibverbs`、provider、kernel RDMA subsystem 的分工。
- device/context/PD/MR/CQ/QP/WR/CQE 的关系。
- `lkey/rkey` 的意义。
- Soft-RoCE 能学习什么，不能证明什么。
- performance tuning 里 batch、inline、selective signaling、CQ polling、CPU affinity/NUMA 各自影响什么。
- one-sided completion、数据可见、业务提交和持久化为什么是四个不同层次。
- wrong-rkey、RNR、retry exceeded、flush error 应该从哪一层开始定位。

先执行文档审计，确认知识入口、相对链接和 Mermaid 围栏完整：

```bash
bash tests/check_fundamentals.sh
```

审计通过 marker：`RDMA_FUNDAMENTALS_COMPLETE`。

## 再跑什么

### 1. RDMA 基础实验总验收

```bash
base=/home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core
for lab in \
  lab-rdma-verbs-object-lifecycle \
  lab-rdma-memory-region-deep-dive \
  lab-rdma-qp-state-machine \
  lab-rdma-rc-pingpong \
  lab-rdma-one-sided-read-write \
  lab-rdma-ud-rocev2-model; do
    make -C "$base/$lab" test || exit 1
done
```

预期：

```text
object lifecycle: pass
MR suite: pass
QP state machine: pass
RC ping-pong: pass
one-sided READ/WRITE: pass
UD/GRH: pass
```

### 2. RC client/server 工程项目

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
SUDO_PASSWORD='<sudo-password>' make test
RDMA_SERVER_CPUSET=0 RDMA_CLIENT_CPUSET=1 make envcheck
```

预期：

```text
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_RKEY_BOUNDARY_PASS
app_runtime_binding role=server ...
app_runtime_binding role=client ...
```

### 3. RDMA performance tuning

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
PERF_SERVER_NUMA_NODE=0 PERF_CLIENT_NUMA_NODE=0 \
SUDO_PASSWORD='<sudo-password>' \
make test
```

预期：

```text
PASS: RDMA SEND latency + batch WR smoke test inline=0 signal_interval=1 poll_budget=16 enable_rtt=0
perf_binding role=server requested_cpuset=0 requested_numa_node=0 ...
perf_binding role=client requested_cpuset=1 requested_numa_node=0 ...
```

## 看什么结果

优先看最终总结和证据索引：

```text
project-rdma-core-summary/README.md
project-rdma-core-summary/EVIDENCE_INDEX.md
project-rdma-core-summary/RDMA_CORE_FINAL_REPORT.md
```

关键测试记录：

```text
project-rdma-rc-client-server/tests/TEST_RECORD_20260712_AFFINITY.md
project-rdma-performance-tuning/tests/TEST_RECORD_20260711.md
project-rdma-performance-tuning/tests/TEST_RECORD_20260712.md
project-rdma-performance-tuning/tests/TEST_RECORD_20260712_CPU_AFFINITY.md
project-rdma-performance-tuning/tests/TEST_RECORD_20260713_NUMACTL_NODE0.md
```

## 当前验收边界

当前验收口径是：

- Soft-RoCE 能验证 RDMA verbs 对象、状态机、协议语义、控制面/数据面流程。
- `project-rdma-rc-client-server` 已补工程结构、错误边界、CPU affinity/NUMA 记录。
- `project-rdma-performance-tuning` 已补 SEND latency、batch WR、inline、selective signaling、CQ polling、RTT、`numactl node0` 绑定。
- 135 只有 `node0`，不能形成跨 NUMA 性能结论。
- 134 登录仍失败，双机 fresh perf 验证暂时受环境阻塞。
- 没有真实 RNIC 时，不把 RXE 结果包装成硬件性能。

## 下一步

当前最自然的主线下一步是：

```text
projects/project-network-acceleration-portfolio
```

目标是把 DPDK、RDMA、eBPF、tc、SmartNIC/DPU 的学习结果整理成作品集、面试讲述和简历材料。
