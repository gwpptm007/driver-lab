# TEST_RECORD_20260712

## 1. 目标

推进 `project-rdma-performance-tuning` 的 batch WR 后续阶段，补齐：

- 可选 RTT phase
- 双机执行脚本入口
- normal / inline RTT 实测记录

## 2. 远端编译命令

主机：`192.168.65.135`

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
make
```

结果：编译通过，生成 `build/rdma-perf-server` 与 `build/rdma-perf-client`。

## 3. normal RTT same-host-over-ens33 验证

先准备 `rxe0 -> ens33`：

```bash
printf 'wq123456!\n' | sudo -S modprobe rdma_rxe >/dev/null 2>&1 || true
printf 'wq123456!\n' | sudo -S rdma link delete rxe0 >/dev/null 2>&1 || true
printf 'wq123456!\n' | sudo -S rdma link add rxe0 type rxe netdev ens33 >/dev/null
rdma link
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+4p'
```

结果摘要：

```text
link rxe0/1 state ACTIVE physical_state LINK_UP netdev ens33
GID[1]: ::ffff:192.168.65.135
```

server：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_ENABLE_RTT=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-server --listen 0.0.0.0 --port 18621 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-server.log 2>&1
```

client：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_ENABLE_RTT=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-client --server 192.168.65.135 --port 18621 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-client.log 2>&1
```

关键结果：

```text
PERF_SEND_LATENCY_SERVER_PASS
PERF_BATCH_SEND_SERVER_PASS
PERF_RTT_LATENCY_SERVER_PASS
cleanup=complete result=pass

perf_result test=send_latency iterations=16 ... avg_ns=9595
perf_result test=batch_send batches=4 ... avg_msg_ns=9589
perf_result test=rtt_latency iterations=16 ... avg_ns=21228
perf_compare single_vs_batch ... speedup_x100=100
perf_compare send_vs_rtt ... send_avg_ns=9595 rtt_avg_ns=21228 rtt_overhead_x100=221
PERF_SEND_LATENCY_CLIENT_PASS
PERF_BATCH_SEND_CLIENT_PASS
PERF_RTT_LATENCY_CLIENT_PASS
cleanup=complete result=pass
```

## 4. inline RTT same-host-over-ens33 验证

server：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_ENABLE_RTT=1 PERF_USE_INLINE=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-server --listen 0.0.0.0 --port 18623 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-inline-server.log 2>&1
```

client：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-performance-tuning
PERF_ENABLE_RTT=1 PERF_USE_INLINE=1 PERF_ITERATIONS=16 PERF_BATCH_SIZE=4 \
  ./build/rdma-perf-client --server 192.168.65.135 --port 18623 \
  --device rxe0 --ib-port 1 --gid-index 1 \
  > tests/manual-rtt-inline-client.log 2>&1
```

关键结果：

```text
PERF_SEND_LATENCY_INLINE_SERVER_PASS
PERF_BATCH_SEND_INLINE_SERVER_PASS
PERF_RTT_LATENCY_INLINE_SERVER_PASS
cleanup=complete result=pass

perf_result test=send_latency_inline iterations=16 ... avg_ns=9117
perf_result test=batch_send_inline batches=4 ... avg_msg_ns=6825
perf_result test=rtt_latency_inline iterations=16 ... avg_ns=9814
perf_compare single_vs_batch ... speedup_x100=133
perf_compare send_vs_rtt ... send_avg_ns=9117 rtt_avg_ns=9814 rtt_overhead_x100=107
PERF_SEND_LATENCY_INLINE_CLIENT_PASS
PERF_BATCH_SEND_INLINE_CLIENT_PASS
PERF_RTT_LATENCY_INLINE_CLIENT_PASS
cleanup=complete result=pass
```

## 5. 双机脚本入口

已新增：

```text
tests/dual_perf_server.sh
tests/dual_perf_client.sh
Makefile: dual-server / dual-client
```

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

可选叠加：

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

## 6. 这次未完成的外部条件

2026-07-12 本次会话中，真实 `134 -> 135` 双机实测仍缺少 `134` 登录权限：

```bash
ssh -o BatchMode=yes -o StrictHostKeyChecking=no wq7@192.168.65.134 "hostname && pwd"
```

返回：

```text
wq7@192.168.65.134: Permission denied (publickey,password).
```

因此当前能确认的是：

- 代码编译通过
- normal RTT 通过
- inline RTT 通过
- 双机脚本入口已落地

但还不能声称“134 -> 135 双机数据已经 fresh PASS”。
