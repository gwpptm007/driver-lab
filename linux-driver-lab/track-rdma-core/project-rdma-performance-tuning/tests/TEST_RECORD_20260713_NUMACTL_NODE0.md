# TEST_RECORD_20260713_NUMACTL_NODE0

## 1. 目标

补齐 `project-rdma-performance-tuning` 的 `numactl` 路径验证：

- 135 上安装并确认 `numactl`
- 记录当前 NUMA 拓扑
- 验证 `PERF_SERVER_NUMA_NODE=0` / `PERF_CLIENT_NUMA_NODE=0`
- 验证 `taskset + numactl` 组合后主 smoke test 仍然 PASS
- 明确边界：当前机器只有 `node0`，不能形成跨 NUMA 性能对比结论

## 2. 测试环境

```text
date=2026-07-13
host=192.168.65.135
user=wq7
repo=/home/wq7/workspace/driver-lab
project=linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
server_cpuset=0
client_cpuset=1
server_numa_node=0
client_numa_node=0
```

## 3. 安装 numactl

执行命令：

```bash
printf '%s\n' 'wq123456!' | sudo -S apt-get update
printf '%s\n' 'wq123456!' | sudo -S apt-get install -y numactl
command -v numactl
numactl --hardware
```

关键结果：

```text
/usr/bin/numactl
available: 1 nodes (0)
node 0 cpus: 0 1 2 3 4 5 6 7
node 0 size: 7894 MB
node 0 free: 5687 MB
```

结论：`numactl` 工具已可用，但当前 Linux NUMA 视角只有 `node0`。

## 4. envcheck 验证

执行命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
PERF_SERVER_NUMA_NODE=0 PERF_CLIENT_NUMA_NODE=0 \
make envcheck
```

关键结果：

```text
env_binding server_cpuset=0 client_cpuset=1 server_numa=0 client_numa=0
CPU NODE SOCKET
available: 1 nodes (0)
node 0 cpus: 0 1 2 3 4 5 6 7
```

结论：环境检查脚本已经能看到 `numactl --hardware`，不再是 `numactl_missing`。

## 5. 同节点 NUMA 绑定 smoke

执行命令：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_SERVER_CPUSET=0 PERF_CLIENT_CPUSET=1 \
PERF_SERVER_NUMA_NODE=0 PERF_CLIENT_NUMA_NODE=0 \
SUDO_PASSWORD='wq123456!' \
make test
```

关键结果：

```text
PASS: RDMA SEND latency + batch WR smoke test inline=0 signal_interval=1 poll_budget=16 enable_rtt=0
```

进程侧绑定证据：

```text
tests/perf-server.log:perf_binding role=server requested_cpuset=0 requested_numa_node=0 current_cpu=0 cpus_allowed=0 mems_allowed=0
tests/perf-client.log:perf_binding role=client requested_cpuset=1 requested_numa_node=0 current_cpu=1 cpus_allowed=1 mems_allowed=0
```

结论：

- `taskset -c 0/1` 生效
- `numactl --cpunodebind=0 --membind=0` 生效
- server/client 都记录到了 `requested_numa_node=0`
- server/client 的 `mems_allowed=0`
- 原有 SEND latency + batch WR smoke 没被 NUMA launcher 打坏

## 6. 边界

这次可以支持：

```text
PASS_NUMACTL_NODE0_BINDING
PASS_CPUSET_AND_NUMA_LAUNCHER_COMBINATION
PASS_PERF_SMOKE_WITH_NUMA_NODE0
```

这次仍不能支持：

```text
PASS_CROSS_NUMA_COMPARISON
```

原因：135 只有 `node0`，没有第二个 NUMA node 可用于同节点 / 跨节点对照。
