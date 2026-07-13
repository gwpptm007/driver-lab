# project-rdma-rc-client-server

这个项目把前面单进程 RDMA verbs 练习推进到“双进程工程化”阶段：
`rdma-rc-server` 和 `rdma-rc-client` 分别独立运行，通过 TCP 控制面交换
`gid/qpn/psn/addr/rkey`，再建立 RC QP 完成 SEND/RECV、RDMA WRITE、RDMA READ
以及若干故障边界验证。

当前已覆盖：
- TCP 控制面 metadata 交换
- RDMA 资源生命周期
- RC QP `RESET -> INIT -> RTR -> RTS`
- RC SEND/RECV
- RDMA WRITE
- RDMA READ
- wrong-rkey 边界
- wrong-addr 边界
- skip-recv / RNR 边界
- disconnect-after-rts 边界
- 双机 Soft-RoCE / RoCEv2 路径
- CPU affinity / NUMA 请求记录与运行时绑定观测

## 构建

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
make clean
make
```

产物：

```text
build/rdma-rc-server
build/rdma-rc-client
```

## 单机测试

135 默认环境：

```bash
SUDO_PASSWORD='wq123456!' make test
```

如果要记录 server/client 绑核：

```bash
RDMA_SERVER_CPUSET=0 RDMA_CLIENT_CPUSET=1 SUDO_PASSWORD='wq123456!' make test
```

如果要额外记录环境拓扑：

```bash
RDMA_SERVER_CPUSET=0 RDMA_CLIENT_CPUSET=1 make envcheck
```

预期 marker：

```text
PASS: TCP control plane metadata exchange
PASS: RDMA resource lifecycle dry-run
PASS: RC client/server SEND WRITE READ wrong-rkey
PASS: wrong-addr remote address boundary
PASS: skip-recv RNR boundary
PASS: disconnect-after-rts cleanup boundary
```

## 双机测试

双机测试使用 `ens33` 上的 IPv4-mapped GID：

```text
135 GID[1] = ::ffff:192.168.65.135
134 GID[1] = ::ffff:192.168.65.134
```

135 server：

```bash
RDMA_SERVER_CPUSET=0 SUDO_PASSWORD='wq123456!' make dual-server
```

134 client：

```bash
RDMA_CLIENT_CPUSET=1 SERVER_IP=192.168.65.135 SUDO_PASSWORD='wq123456' make dual-client
```

双机脚本会写入：

```text
tests/server-dual.log
tests/client-dual.log
tests/tcpdump-dual-4791.log
```

## 运行时绑定记录

脚本层通过 `tests/launch_helpers.sh` 统一封装：
- `RDMA_SERVER_CPUSET` / `RDMA_CLIENT_CPUSET`
- `RDMA_SERVER_NUMA_NODE` / `RDMA_CLIENT_NUMA_NODE`
- `taskset -c ...`
- `numactl --cpunodebind=... --membind=...`

应用层会打印：

```text
app_runtime_binding role=server requested_cpuset=0 requested_numa_node=- current_cpu=0 cpus_allowed=0 mems_allowed=0
app_runtime_binding role=client requested_cpuset=1 requested_numa_node=- current_cpu=1 cpus_allowed=1 mems_allowed=0
```

这两行的价值在于把“脚本请求”和“进程实际约束”放到同一份日志里，后续做
双机 latency / throughput、跨 NUMA 对比、软中断/队列亲和复盘时，不需要再猜
当时的运行约束。

## 当前边界

Soft-RoCE 能证明：
- verbs 编程模型
- RC 控制面/数据面闭环
- RoCEv2 UDP 4791 网络路径
- 绑核请求是否真正落到进程约束上

Soft-RoCE 不能直接代表真实 RNIC 的：
- DMA/offload 性能
- PCIe/NUMA 实际跨节点代价
- PFC/ECN/拥塞控制行为

后续更自然的下一步是把这套绑定记录迁移到真实 perf 场景里，继续做：
- latency / throughput
- inline data
- batch WR
- selective signaling
- CQ polling 策略
- 真实多 NUMA 机器上的跨节点对比

## 文档入口

```text
docs/ARCHITECTURE.md           # 分层结构、CPU 亲和/NUMA 记录链路、Mermaid/UML
docs/AFFINITY_NUMA_RECORDING.md # 绑核/绑节点记录原理与字段解释
docs/CONTROL_AND_DATA_PLANE.md # 控制面 / 数据面职责
docs/DEEP_LEARNING.md          # 原理细节、关键机制、时序图
docs/TEST_FLOW.md              # 完整测试流程与排障路径
tests/TEST_RECORD_20260711.md  # 单机/双机/绑核实测记录
tests/TEST_RECORD_20260712_AFFINITY.md # CPU affinity / NUMA 记录阶段实测
```
