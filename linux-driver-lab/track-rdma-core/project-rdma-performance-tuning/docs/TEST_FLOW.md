# TEST_FLOW

## 1. 构建

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
make clean
make
```

## 2. 准备 Soft-RoCE

135 默认环境：

```bash
sudo modprobe rdma_rxe
sudo ip -6 addr add fe80::34/64 dev ens34 2>/dev/null || true
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens34
while ip -6 addr show ens34 | grep -q tentative; do sleep 1; done
```

检查：

```bash
rdma link
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

推荐直接用：

```bash
make envcheck
```

## 3. 自动化 smoke test

```bash
SUDO_PASSWORD='wq123456!' make test
```

预期：

```text
PASS: RDMA SEND latency + batch WR smoke test
```

## 4. 手工运行

server：

```bash
PERF_ITERATIONS=1000 PERF_BATCH_SIZE=8 ./build/rdma-perf-server --listen 127.0.0.1 --port 18600 --device rxe0 --ib-port 1 --gid-index 1
```

client：

```bash
PERF_ITERATIONS=1000 PERF_BATCH_SIZE=8 ./build/rdma-perf-client --server 127.0.0.1 --port 18600 --device rxe0 --ib-port 1 --gid-index 1
```

client 输出示例：

```text
perf_result test=send_latency iterations=1000 avg_ns=<n> min_ns=<n> p50_ns=<n> p95_ns=<n> p99_ns=<n> max_ns=<n>
perf_result test=batch_send batches=125 batch_size=8 messages=1000 avg_batch_ns=<n> avg_msg_ns=<n> min_batch_ns=<n> p50_batch_ns=<n> p95_batch_ns=<n> p99_batch_ns=<n> max_batch_ns=<n>
perf_throughput test=batch_send messages=1000 total_ns=<n> msg_per_sec=<n>
perf_compare single_vs_batch single_avg_ns=<n> batch_avg_msg_ns=<n> speedup_x100=<n>
PERF_SEND_LATENCY_CLIENT_PASS
PERF_BATCH_SEND_CLIENT_PASS
```

## 5. 日志文件

```text
tests/perf-server.log
tests/perf-client.log
```

重点看：

```text
app_config
perf_config
phase=resources_create
phase=qp_to_rts
server_perf_recv_cqe
client_perf_send_cqe
server_perf_batch_recv_cqe
client_perf_batch_send_cqe
perf_result
perf_throughput
perf_compare
```

## 6. batch WR 专项验证命令

小样本开发验证：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make quickreport
```

阶段收口验证：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' make fullreport
```

需要临时调整规模时：

```bash
SUDO_PASSWORD='wq123456!' make quickcheck QUICK_ITERATIONS=32 QUICK_BATCH_SIZE=8
SUDO_PASSWORD='wq123456!' make fullcheck FULL_ITERATIONS=2000 FULL_BATCH_SIZE=8
SUDO_PASSWORD='wq123456!' make inlinequickreport
SUDO_PASSWORD='wq123456!' make inlinefullreport
SUDO_PASSWORD='wq123456!' make selectivequickreport
SUDO_PASSWORD='wq123456!' make selectivefullreport
SUDO_PASSWORD='wq123456!' make selectiveinlinequickreport
SUDO_PASSWORD='wq123456!' make selectiveinlinefullreport
SUDO_PASSWORD='wq123456!' make sweep SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make selectivesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make inlinesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make selectiveinlinesweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make signalreport SIGNAL_INTERVALS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make inlinesignalreport SIGNAL_INTERVALS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make pollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make inlinepollreport POLL_CQ_BUDGETS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make sweepreport SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
make comparesweeps
make compareselective
make compareinlineselective
SUDO_PASSWORD='wq123456!' make batchphaseclose SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make selectivephaseclose SELECTIVE_SIGNAL_INTERVAL=4 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make selectiveinlinephaseclose SELECTIVE_SIGNAL_INTERVAL=4 SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
SUDO_PASSWORD='wq123456!' make finalphaseclose SIGNAL_INTERVALS="1 2 4 8 16" SWEEP_ITERATIONS=1000 SWEEP_BATCH_SIZES="1 2 4 8 16"
```

只 grep marker，避免打开完整日志：

```bash
make markers
```

通过标准：

```text
PERF_SEND_LATENCY_SERVER_PASS
PERF_SEND_LATENCY_CLIENT_PASS
PERF_BATCH_SEND_SERVER_PASS
PERF_BATCH_SEND_CLIENT_PASS
perf_result test=send_latency
perf_result test=batch_send
perf_throughput test=batch_send
perf_compare single_vs_batch
cleanup=complete result=pass
```

inline 模式额外关注：

```text
PERF_SEND_LATENCY_INLINE_SERVER_PASS
PERF_SEND_LATENCY_INLINE_CLIENT_PASS
PERF_BATCH_SEND_INLINE_SERVER_PASS
PERF_BATCH_SEND_INLINE_CLIENT_PASS
perf_result test=send_latency_inline
perf_result test=batch_send_inline
perf_compare single_vs_batch inline=on
```

selective signaling 模式额外关注：

```text
PERF_BATCH_SEND_SELECTIVE_SERVER_PASS
PERF_BATCH_SEND_SELECTIVE_CLIENT_PASS
perf_result test=batch_send_selective
perf_compare single_vs_batch signal_mode=selective signal_interval=4
```

signal interval 矩阵额外关注：

```text
signal_interval_sweep_step status=done signal_interval=<N>
signal_interval_sweep=pass
signal_interval_summary=pass
signal_interval_check=pass
```

CQ polling budget 矩阵额外关注：

```text
poll_budget_sweep_step status=done poll_budget=<N>
poll_budget_sweep=pass
poll_budget_summary=pass
poll_budget_check=pass
```

CSV 产物：

```text
tests/perf-summary.csv
tests/perf-sweep.csv
tests/perf-sweep-inline.csv
tests/perf-sweep-sig4.csv
tests/perf-sweep-inline-sig4.csv
tests/perf-signal-interval-sweep.csv
tests/perf-signal-interval-inline-sweep.csv
tests/perf-poll-budget-sweep.csv
tests/perf-poll-budget-inline-sweep.csv
tests/perf-sweep-summary.md
tests/perf-sweep-inline-summary.md
tests/perf-sweep-sig4-summary.md
tests/perf-signal-interval-summary.md
tests/perf-signal-interval-inline-summary.md
tests/perf-poll-budget-summary.md
tests/perf-poll-budget-inline-summary.md
tests/perf-inline-vs-normal-summary.md
tests/perf-selective-vs-all-summary.md
tests/perf-inline-selective-vs-inline-summary.md
tests/sweep/
tests/sweep-inline/
tests/sweep-sig4/
tests/sweep-inline-sig4/
```

phase close 时可额外校验：

```text
compare_report_check=pass
```

数值断言：

```text
send_latency avg_ns > 0
batch_send avg_msg_ns > 0
batch_send msg_per_sec > 0
```

## 7. RTT 验证命令

如果当前主机的 `tests/perf_smoke_test.sh` 默认网卡/GID 口径不适合 RTT 验证，可以直接手工跑 `ens33 + gid-index=1`：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning

printf 'wq123456!\n' | sudo -S modprobe rdma_rxe >/dev/null 2>&1 || true
printf 'wq123456!\n' | sudo -S rdma link delete rxe0 >/dev/null 2>&1 || true
printf 'wq123456!\n' | sudo -S rdma link add rxe0 type rxe netdev ens33

PERF_ENABLE_RTT=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-server --listen 0.0.0.0 --port 18621 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-server.log 2>&1
```

另一终端：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning

PERF_ENABLE_RTT=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-client --server 192.168.65.135 --port 18621 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-client.log 2>&1
```

inline RTT：

```bash
PERF_ENABLE_RTT=1 PERF_USE_INLINE=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-server --listen 0.0.0.0 --port 18623 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-inline-server.log 2>&1

PERF_ENABLE_RTT=1 PERF_USE_INLINE=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-client --server 192.168.65.135 --port 18623 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-inline-client.log 2>&1
```

关键 marker：

```text
perf_result test=rtt_latency
perf_result test=rtt_latency_inline
perf_compare send_vs_rtt
PERF_RTT_LATENCY_SERVER_PASS
PERF_RTT_LATENCY_CLIENT_PASS
PERF_RTT_LATENCY_INLINE_SERVER_PASS
PERF_RTT_LATENCY_INLINE_CLIENT_PASS
```

## 8. 双机脚本入口

135 server：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SUDO_PASSWORD='wq123456!' ENABLE_TCPDUMP=0 PERF_ENABLE_RTT=1 make dual-server
```

134 client：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
SERVER_IP=192.168.65.135 SUDO_PASSWORD='<134-sudo-password>' PERF_ENABLE_RTT=1 make dual-client
```

若要叠加现有调优变量，直接透传：

```bash
SERVER_IP=192.168.65.135 \
PERF_BATCH_SIZE=16 \
PERF_USE_INLINE=1 \
PERF_SIGNAL_INTERVAL=4 \
PERF_POLL_CQ_BUDGET=1 \
PERF_ENABLE_RTT=1 \
SUDO_PASSWORD='<134-sudo-password>' \
make dual-client
```

## 9. CPU affinity / NUMA 记录命令

先看机器拓扑与能力边界：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
bash tests/check_env.sh
```

最小绑核样本：

```bash
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 PERF_SKIP_CLEAN=1 \
SUDO_PASSWORD='wq123456!' \
bash tests/perf_smoke_test.sh
```

若系统装有 `numactl`，可额外指定：

```bash
PERF_SERVER_NUMA_NODE=0 PERF_CLIENT_NUMA_NODE=0 \
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
SUDO_PASSWORD='wq123456!' \
bash tests/perf_smoke_test.sh
```

关键 marker：

```text
script_binding role=server
script_binding role=client
perf_binding role=server
perf_binding role=client
cpus_allowed=
mems_allowed=
```
