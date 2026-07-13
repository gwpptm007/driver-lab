# TEST_RECORD_20260712_CPU_AFFINITY

## 1. 目标

验证 `project-rdma-performance-tuning` 的 CPU affinity / NUMA 记录阶段：

- launcher 能传入 `taskset`
- 进程能打印 `perf_binding`
- 原有 smoke test 仍然 PASS

## 2. 135 环境拓扑命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
nproc
lscpu | sed -n '1,20p'
bash tests/check_env.sh
```

关键结果：

```text
8
env_binding server_cpuset=auto client_cpuset=auto server_numa=auto client_numa=auto
CPU NODE SOCKET
env_step=numactl_hardware
numactl_missing
```

结论：

- 可用 CPU 为 `0-7`
- 当前只有 `node0`
- `numactl` 未安装

## 3. fresh 编译

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
make clean
make
```

结果：编译通过。

## 4. fresh 绑核 smoke

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 PERF_SKIP_CLEAN=1 \
SUDO_PASSWORD='wq123456!' \
bash tests/perf_smoke_test.sh
```

脚本侧结果：

```text
script_binding role=server requested_cpuset=0 requested_numa_node=auto
script_binding role=client requested_cpuset=1 requested_numa_node=auto
PASS: RDMA SEND latency + batch WR smoke test inline=0 signal_interval=1 poll_budget=16 enable_rtt=0
```

进程侧关键结果：

```text
tests/perf-server.log:perf_binding role=server requested_cpuset=0 requested_numa_node=- current_cpu=0 cpus_allowed=0 mems_allowed=0
tests/perf-client.log:perf_binding role=client requested_cpuset=1 requested_numa_node=- current_cpu=1 cpus_allowed=1 mems_allowed=0
tests/perf-server.log:PERF_SEND_LATENCY_SERVER_PASS
tests/perf-server.log:PERF_BATCH_SEND_SERVER_PASS
tests/perf-client.log:PERF_SEND_LATENCY_CLIENT_PASS
tests/perf-client.log:PERF_BATCH_SEND_CLIENT_PASS
tests/perf-server.log:cleanup=complete result=pass
tests/perf-client.log:cleanup=complete result=pass
```

## 5. 结论

2026-07-12 这轮 fresh 证据可以支持：

- CPU affinity 入口已经落地
- 绑核结果能从进程日志里看到
- 绑核功能没有破坏当前 RDMA perf 基线

但这轮不能支持：

- NUMA 对比已经完成
- `numactl` 路径已经 fresh PASS

原因很直接：`192.168.65.135` 当前环境 `numactl_missing`，而且 `lscpu -e` 只看到 `node0`。
