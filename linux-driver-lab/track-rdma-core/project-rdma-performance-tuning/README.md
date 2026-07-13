# project-rdma-performance-tuning

这个项目进入 RDMA 性能与调优阶段。它不再只验证“能不能跑通”，而是开始回答：

- 一次 SEND completion 大概需要多久？
- CQ polling、batch、inline、selective signaling 会影响什么？
- Soft-RoCE 能说明哪些机制，不能说明哪些真实 RNIC 性能？

第一版先落一个最小可复现基线：

- 独立 `rdma-perf-server` / `rdma-perf-client`。
- 复用上一项目的 TCP 控制面和 RDMA resource helper。
- server 预先 post RECV，client 测量 `ibv_post_send()` 到 SEND CQE 的耗时。
- 输出 avg/min/max/p50/p95/p99。

## 构建

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
make clean
make
```

## 单机 smoke test

135 默认环境：

```bash
SUDO_PASSWORD='wq123456!' make test
```

预期：

```text
PASS: RDMA SEND latency smoke test
```

## 手工运行

server：

```bash
PERF_ITERATIONS=1000 ./build/rdma-perf-server --listen 127.0.0.1 --port 18600 --device rxe0 --ib-port 1 --gid-index 1
```

client：

```bash
PERF_ITERATIONS=1000 ./build/rdma-perf-client --server 127.0.0.1 --port 18600 --device rxe0 --ib-port 1 --gid-index 1
```

## 当前边界

当前 latency 指标是 client 侧 `post_send -> SEND CQE` 的软件 RXE 基线，不等于真实 RNIC 线速延迟，也不是完整业务 RTT。

当前已经完成：

- single SEND completion latency 基线。
- batch WR single vs batch 对比。
- inline data 的 normal vs inline 并行实验入口。
- selective signaling 的 batch SEND 实验入口。
- client 侧 CQ polling budget 矩阵扫描与 summary 产物链路。
- signal interval 矩阵扫描与 summary 产物链路。
- sweep / CSV / summary / compare report 产物链路。

当前阶段已经收口；以下内容改为下一个专题，不再算作本项目未完成项：

- 双机模式。
- CPU affinity / NUMA 记录。

## batch WR 阶段

本阶段在同一条 RC QP 上保留 single SEND completion latency 基线，并追加 batch SEND 对比：

- single：每轮 `post SEND -> poll 1 个 SEND CQE -> 等 server ITER_DONE`。
- batch：server 一次链式 post N 个 RECV，client 一次 `ibv_post_send()` 提交 N 个 SEND WR，然后 poll N 个 SEND CQE。
- 默认 `PERF_BATCH_SIZE=8`，最大限制为 16，匹配当前公共 helper 创建 QP 时的 `max_send_wr/max_recv_wr=16`。
- 输出 `perf_result test=batch_send`、`perf_throughput test=batch_send` 和 `perf_compare single_vs_batch`。

示例：

```bash
SUDO_PASSWORD='wq123456!' make quickcheck
SUDO_PASSWORD='wq123456!' make fullcheck
SUDO_PASSWORD='wq123456!' make sweep
make help
```

需要临时改样本规模时，可以直接覆盖：

```bash
SUDO_PASSWORD='wq123456!' make quickcheck QUICK_ITERATIONS=32 QUICK_BATCH_SIZE=8
SUDO_PASSWORD='wq123456!' make sweep SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

切环境或换内核时，先看环境摘要：

```bash
make envcheck
```

推荐的一键入口：

```bash
SUDO_PASSWORD='wq123456!' make quickdiag
SUDO_PASSWORD='wq123456!' make fulldiag
SUDO_PASSWORD='wq123456!' make quickreport
SUDO_PASSWORD='wq123456!' make fullreport
SUDO_PASSWORD='wq123456!' make inlinequickreport
SUDO_PASSWORD='wq123456!' make inlinefullreport
SUDO_PASSWORD='wq123456!' make selectivequickreport
SUDO_PASSWORD='wq123456!' make selectivefullreport
SUDO_PASSWORD='wq123456!' make selectiveinlinequickreport
SUDO_PASSWORD='wq123456!' make selectiveinlinefullreport
SUDO_PASSWORD='wq123456!' make sweep
SUDO_PASSWORD='wq123456!' make selectivesweepreport
SUDO_PASSWORD='wq123456!' make inlinesweepreport
SUDO_PASSWORD='wq123456!' make selectiveinlinesweepreport
SUDO_PASSWORD='wq123456!' make signalreport
SUDO_PASSWORD='wq123456!' make inlinesignalreport
SUDO_PASSWORD='wq123456!' make pollreport
SUDO_PASSWORD='wq123456!' make inlinepollreport
SUDO_PASSWORD='wq123456!' make sweepreport
make comparesweeps
make compareselective
make compareinlineselective
SUDO_PASSWORD='wq123456!' make batchphaseclose SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make selectivephaseclose SELECTIVE_SIGNAL_INTERVAL=4 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make selectiveinlinephaseclose SELECTIVE_SIGNAL_INTERVAL=4 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make finalphaseclose SIGNAL_INTERVALS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

预期关键 marker：

```text
PERF_SEND_LATENCY_CLIENT_PASS
PERF_BATCH_SEND_CLIENT_PASS
perf_compare single_vs_batch single_avg_ns=<n> batch_avg_msg_ns=<n> speedup_x100=<n>
PASS: RDMA SEND latency + batch WR smoke test
```

## inline data 阶段

在 batch WR 基线之外，当前新增了可选的 inline 模式：

- 通过 `PERF_USE_INLINE=1` 打开。
- single SEND 结果名变为 `send_latency_inline`。
- batch SEND 结果名变为 `batch_send_inline`。
- `perf_compare single_vs_batch` 会追加 `inline=on/off` 字段。

推荐入口：

```bash
SUDO_PASSWORD='wq123456!' make inlinequickreport
SUDO_PASSWORD='wq123456!' make inlinefullreport
```

## selective signaling 阶段

当前 selective signaling 只作用在 batch SEND 路径，single SEND baseline 仍保持每条都 signaled，方便与旧数据直接对照。

- 通过 `PERF_SIGNAL_INTERVAL=<N>` 打开 selective signaling。
- `PERF_SIGNAL_INTERVAL=1` 表示保持当前 all-signaled 行为。
- `PERF_SIGNAL_INTERVAL=4` 表示每 4 条 WR 产生一个 SEND CQE，同时批尾 WR 必定 signaled。
- batch 结果名会切到 `batch_send_selective` 或 `batch_send_inline_selective`。
- `perf_result` / `perf_throughput` / `perf_compare` 会追加 `signal_mode`、`signal_interval`、`signaled_total` 字段。

推荐入口：

```bash
SUDO_PASSWORD='wq123456!' make selectivequickreport
SUDO_PASSWORD='wq123456!' make selectivefullreport
SUDO_PASSWORD='wq123456!' make selectivesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make compareselective
```

若要同时叠加 inline + selective，现在既可以直接用目标，也可以覆盖环境变量：

```bash
SUDO_PASSWORD='wq123456!' make selectiveinlinequickreport
SUDO_PASSWORD='wq123456!' make selectiveinlinesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make compareinlineselective

PERF_USE_INLINE=1 PERF_SIGNAL_INTERVAL=4 SUDO_PASSWORD='wq123456!' make quickreport
SWEEP_USE_INLINE=1 SWEEP_SIGNAL_INTERVAL=4 SUDO_PASSWORD='wq123456!' make sweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

## signal interval 矩阵阶段

在 selective signaling 已经接通之后，当前项目最后一段收口不是再加新功能，而是把 `PERF_SIGNAL_INTERVAL` 当作实验维度做矩阵扫描。

- normal 路径入口：`make signalreport`
- inline 路径入口：`make inlinesignalreport`
- 输出矩阵：`tests/perf-signal-interval-sweep.csv`、`tests/perf-signal-interval-inline-sweep.csv`
- 输出摘要：`tests/perf-signal-interval-summary.md`、`tests/perf-signal-interval-inline-summary.md`

135 上 2026-07-12 已验证：

- normal 最佳 interval 是 `16`，最佳点位 `batch_size=16`，`msg_per_sec=258825`，`batch_avg_msg_ns=3863`，`speedup_x100=168`。
- inline 最佳 interval 是 `8`，最佳点位 `batch_size=16`，`msg_per_sec=276909`，`batch_avg_msg_ns=3611`，`speedup_x100=173`。

这说明在当前 RXE + 小消息 + 当前 polling 写法下，减少 CQE 数量仍然能继续改善 batch SEND 的完成路径；但最佳 interval 不是固定常数，需要结合 inline 与 batch size 一起扫描。

## CQ polling budget 阶段

signal interval 收口之后，当前项目继续把 `ibv_poll_cq()` 本身拉成一个单独实验维度，但仍然保持最小边界：只比较 client 侧 SEND CQE 的取回预算，不引入 event channel、sleep/backoff，也不同时改 server 侧 RECV polling。

- 环境变量：`PERF_POLL_CQ_BUDGET=<N>`
- `N=1`：每次 `ibv_poll_cq()` 最多取 1 个 CQE，对应 `poll_mode=single`
- `N>1`：每次 `ibv_poll_cq()` 最多取 `N` 个 CQE，对应 `poll_mode=burst`
- 默认 `N=16`，等价于当前 batch 路径的默认收敛行为

推荐入口：

```bash
SUDO_PASSWORD='wq123456!' make pollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make inlinepollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

135 上 2026-07-12 已验证：

- normal 最佳 throughput / latency budget 都是 `16`，最佳点位 `batch_size=16`，`msg_per_sec=241605`，`batch_avg_msg_ns=4138`
- normal 最佳 speedup budget 是 `1`，最佳点位 `batch_size=16`，`speedup_x100=157`
- inline 最佳 budget 是 `1`，最佳点位 `batch_size=16`，`msg_per_sec=275508`，`batch_avg_msg_ns=3629`，`speedup_x100=170`

这个结果说明：在当前 RXE 单机场景里，poll budget 并不是单调关系。normal 模式下，大 budget 在 throughput/折算延迟上更占优；inline 模式下，`budget=1` 反而拿到了更好的 throughput 和 speedup。

## RTT / 双机入口阶段

这一阶段在不破坏原有 completion-latency 口径的前提下，追加了一个可选 request/response RTT phase：

- completion latency 仍然测 `post SEND -> local SEND CQE`
- RTT 通过 `PERF_ENABLE_RTT=1` 打开，测 `request SEND 提交 -> response RECV CQE`
- RTT 结果名为 `rtt_latency` 或 `rtt_latency_inline`
- 额外输出 `perf_compare send_vs_rtt`
- 双机脚本复用 `ens33 + gid-index=1 + rxe0` 路径

2026-07-12 在 `192.168.65.135` 的 fresh 实测：

- normal RTT：`send_avg_ns=9595`，`batch_avg_msg_ns=9589`，`rtt_avg_ns=21228`
- inline RTT：`send_avg_ns=9117`，`batch_avg_msg_ns=6825`，`rtt_avg_ns=9814`

双机脚本已经落地，但这次会话里对 `192.168.65.134` 的 SSH 登录仍返回 `Permission denied (publickey,password)`，因此真实 `134 -> 135` 双机结果还没有 fresh 证据。

双机执行命令：

```bash
# 135
SUDO_PASSWORD='wq123456!' ENABLE_TCPDUMP=0 PERF_ENABLE_RTT=1 make dual-server

# 134
SERVER_IP=192.168.65.135 SUDO_PASSWORD='<134-sudo-password>' PERF_ENABLE_RTT=1 make dual-client
```

## CPU affinity / NUMA 记录阶段

这一阶段不改 RDMA 数据路径，只补“实验变量记录能力”：

- `PERF_SERVER_CPUSET` / `PERF_CLIENT_CPUSET`
- `PERF_SERVER_NUMA_NODE` / `PERF_CLIENT_NUMA_NODE`
- `perf_binding role=... current_cpu=... cpus_allowed=... mems_allowed=...`

2026-07-12 在 `192.168.65.135` 的 fresh 绑核样本：

```bash
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 PERF_SKIP_CLEAN=1 \
SUDO_PASSWORD='wq123456!' \
bash tests/perf_smoke_test.sh
```

关键结果：

```text
perf_binding role=server requested_cpuset=0 requested_numa_node=- current_cpu=0 cpus_allowed=0 mems_allowed=0
perf_binding role=client requested_cpuset=1 requested_numa_node=- current_cpu=1 cpus_allowed=1 mems_allowed=0
```

当前边界也要写清楚：`135` 已补装 `numactl`，同节点 `NUMA_NODE=0` 绑定已 fresh PASS；但机器仍只看到 `node0`，所以这轮完成的是“NUMA 绑定路径验证”，不是“跨 NUMA 对比结论”。

## 文档入口

```text
docs/ARCHITECTURE.md      # 项目结构和测量边界
docs/DEEP_LEARNING.md     # 性能原理、Mermaid 图、调优模型
docs/TEST_FLOW.md         # 每一步测试命令
docs/RTT_DUAL_HOST.md     # RTT 设计、双机入口、UML / Mermaid
docs/CPU_AFFINITY_NUMA.md # 绑核/NUMA 原理、Mermaid 图、能力边界
tests/TEST_RECORD_20260711.md
tests/TEST_RECORD_20260712.md
tests/TEST_RECORD_20260712_CPU_AFFINITY.md
tests/TEST_RECORD_20260713_NUMACTL_NODE0.md
```
