# TEST_RECORD_20260712_AFFINITY

## 1. 目标

本次记录验证 `project-rdma-rc-client-server` 新增的 CPU affinity / NUMA 记录链路：

- 脚本能接受 `RDMA_SERVER_CPUSET` / `RDMA_CLIENT_CPUSET`
- 双角色启动前能打印 `script_binding`
- 应用启动后能打印 `app_runtime_binding`
- 单机完整用例在绑核场景下仍然全部通过
- 环境检查脚本能输出 CPU / socket / NUMA 拓扑

## 2. 测试环境

```text
date=2026-07-12
host=192.168.65.135
user=wq7
repo=/home/wq7/workspace/driver-lab
project=linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
single-node netdev=ens34
soft_roce=rxe0
server_cpuset=0
client_cpuset=1
```

补充观察：

```text
taskset=present
numactl=missing
lscpu topology=visible
```

## 3. 执行命令

### 3.1 编译

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
make clean
make
```

### 3.2 环境拓扑检查

```bash
RDMA_SERVER_CPUSET=0 RDMA_CLIENT_CPUSET=1 make envcheck
```

### 3.3 最小红绿验证

先确认 `control-plane-only` 日志里出现 `app_runtime_binding`：

```bash
./build/rdma-rc-server --control-plane-only --listen 127.0.0.1 --port 19515 > tests/tdd-server.log 2>&1 &
server_pid=$!
sleep 1
./build/rdma-rc-client --control-plane-only --server 127.0.0.1 --port 19515 > tests/tdd-client.log 2>&1
wait "${server_pid}"
grep -n 'app_runtime_binding' tests/tdd-server.log tests/tdd-client.log
```

### 3.4 单机完整脚本 + 绑核

```bash
RDMA_SERVER_CPUSET=0 RDMA_CLIENT_CPUSET=1 SUDO_PASSWORD='wq123456!' make test
```

### 3.5 收口 grep

```bash
grep -n 'script_binding\|app_runtime_binding\|PASS:' \
  tests/server.log \
  tests/client.log \
  tests/server-dry-run.log \
  tests/client-dry-run.log \
  tests/server-full.log \
  tests/client-full.log
```

## 4. 关键结果

### 4.1 最小验证

```text
tests/tdd-server.log:2:app_runtime_binding role=server requested_cpuset=- requested_numa_node=- current_cpu=2 cpus_allowed=0-127 mems_allowed=0
tests/tdd-client.log:2:app_runtime_binding role=client requested_cpuset=- requested_numa_node=- current_cpu=1 cpus_allowed=0-127 mems_allowed=0
```

结论：共享绑定日志函数已接入 server/client 启动路径。

### 4.2 `make envcheck`

关键检查点：

```text
script_binding role=server requested_cpuset=0 requested_numa_node=auto
script_binding role=client requested_cpuset=1 requested_numa_node=auto
CPU NODE SOCKET
numactl_missing=1
```

结论：CPU/socket 拓扑可见，但当前机器未安装 `numactl`，只能做绑核记录，不能做真实
NUMA bind 对比。

### 4.3 `make test`

预期 PASS 全部出现：

```text
PASS: TCP control plane metadata exchange
PASS: RDMA resource lifecycle dry-run
PASS: RC client/server SEND WRITE READ wrong-rkey
PASS: wrong-addr remote address boundary
PASS: skip-recv RNR boundary
PASS: disconnect-after-rts cleanup boundary
```

关键绑定 marker 应出现于 server/client 各阶段日志：

```text
script_binding role=server requested_cpuset=0 requested_numa_node=auto
script_binding role=client requested_cpuset=1 requested_numa_node=auto
app_runtime_binding role=server requested_cpuset=0 requested_numa_node=- current_cpu=0 cpus_allowed=0 mems_allowed=0
app_runtime_binding role=client requested_cpuset=1 requested_numa_node=- current_cpu=1 cpus_allowed=1 mems_allowed=0
```

## 5. 结果解释

### 5.1 为什么 `script_binding` 和 `app_runtime_binding` 要同时保留

- `script_binding` 证明“测试脚本请求了什么”
- `app_runtime_binding` 证明“进程实际看到了什么”

二者放在一起，后续复盘时就能区分“请求没发出去”和“请求发了但没生效”。

### 5.2 为什么现在不能宣称 NUMA 对比已经完成

因为当前 135 环境：
- 没安装 `numactl`
- 记录里只有 `node0`

所以现在完成的是“NUMA 记录入口”和“绑核证据链”，不是“跨 NUMA 性能对比”。

## 6. 结论

本次阶段可归纳为：

```text
PASS_CPU_AFFINITY_RECORD
PASS_RUNTIME_BINDING_LOG
PASS_SINGLE_HOST_REGRESSION
BLOCKED_NUMA_COMPARISON_BY_ENV
```

下一步如果要继续往性能方向推进，优先顺序建议是：

1. 在有 `numactl` 的环境补 `RDMA_*_NUMA_NODE` 真正绑定验证
2. 再把这套证据链迁移到 `project-rdma-performance-tuning`
3. 最后做 single/batch、RTT、CQ polling 在不同绑定下的对比
