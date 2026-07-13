# TEST_FLOW

本文记录 `project-rdma-rc-client-server` 的单机与双机测试流程。目标不是只给一条 `make test`，而是让每一步都能解释“为什么执行、在验证什么、失败时查哪里”。

## 1. 测试机信息

135：

```text
host=192.168.65.135
user=wq7
password=wq123456!
single-node netdev=ens34
dual-node netdev=ens33
repo=/home/wq7/workspace/driver-lab
project=linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
```

134：

```text
host=192.168.65.134
user=wq
password=wq123456
dual-node netdev=ens33
repo=/home/wq/workspace/driver-lab
project=linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
```

## 2. 构建

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
make clean
make
```

预期产物：

```text
build/rdma-rc-server
build/rdma-rc-client
```

## 3. TCP 控制面单独测试

server：

```bash
./build/rdma-rc-server --control-plane-only --listen 127.0.0.1 --port 18515
```

client：

```bash
./build/rdma-rc-client --control-plane-only --server 127.0.0.1 --port 18515
```

预期：

```text
server_control_plane=pass
client_control_plane=pass
TCP_CONTROL_PLANE_PASS
```

这个阶段不需要 RDMA device。若失败，优先查 TCP socket、端口占用、metadata 文本解析。

## 4. 单机 Soft-RoCE 准备

135 默认使用 `ens34` 和显式 link-local 地址 `fe80::34`：

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
ibv_devices
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

预期：

```text
link rxe0/1 state ACTIVE physical_state LINK_UP netdev ens34
rxe0
GID[  1]: fe80::34, RoCE v2
```

## 5. RDMA resource dry-run

server：

```bash
./build/rdma-rc-server --dry-run --device rxe0 --ib-port 1 --gid-index 1
```

client：

```bash
./build/rdma-rc-client --dry-run --device rxe0 --ib-port 1 --gid-index 1
```

预期：

```text
rdma_resources=created
cleanup=complete result=pass
```

这个阶段验证：

- `ibv_get_device_list`
- `ibv_open_device`
- `ibv_query_port`
- `ibv_query_gid`
- `ibv_alloc_pd`
- `ibv_reg_mr`
- `ibv_create_cq`
- `ibv_create_qp`
- 反向资源清理

## 6. 单机完整自动化测试

135：

```bash
SUDO_PASSWORD='wq123456!' make test
```

134：

```bash
RDMA_NETDEV=ens33 RDMA_GID_ADDR=fe80::134 RDMA_GID_INDEX=2 SUDO_PASSWORD='wq123456' make test
```

预期：

```text
PASS: TCP control plane metadata exchange
PASS: RDMA resource lifecycle dry-run
PASS: RC client/server SEND WRITE READ wrong-rkey
PASS: wrong-addr remote address boundary
PASS: skip-recv RNR boundary
PASS: disconnect-after-rts cleanup boundary
```

日志文件：

```text
tests/server.log
tests/client.log
tests/server-dry-run.log
tests/client-dry-run.log
tests/server-full.log
tests/client-full.log
tests/server-wrong-addr.log
tests/client-wrong-addr.log
tests/server-skip-recv.log
tests/client-skip-recv.log
tests/server-disconnect.log
tests/client-disconnect.log
```

脚本 stdout 会打印 `script_*` 日志：

```text
script_config name=client_server_test device=rxe0 ib_port=1 netdev=ens34 gid_addr=fe80::34 gid_index=1
script_case=control_plane status=start server_log=tests/server.log client_log=tests/client.log
script_step=prepare_rxe status=start netdev=ens34 device=rxe0 gid_addr=fe80::34
script_case=full_wrong_rkey status=pass
script_summary name=client_server_test status=pass
```

程序日志文件中会打印 `app_config` 和 `phase`：

```text
app_config role=server mode=full listen=127.0.0.1 server=127.0.0.1 port=18516 device=rxe0 ib_port=1 gid_index=1 flags=
phase=resources_create role=server status=start
phase=metadata_exchange role=server status=done
phase=qp_to_rts role=server status=done
phase=rdma_write role=server status=done
phase=fault_boundary role=server case=wrong_rkey status=done
```

这些日志的用途：

- `script_config`：确认脚本实际使用的网卡、GID、端口。
- `script_case`：确认当前跑到哪个测试场景。
- `app_config`：确认 server/client 进程自己的命令行参数。
- `phase`：确认程序卡在资源创建、TCP 控制面、QP 状态迁移还是数据面。

## 7. 故障边界手工测试

### wrong-addr

server：

```bash
./build/rdma-rc-server --listen 127.0.0.1 --port 18519 --device rxe0 --ib-port 1 --gid-index 1 --wrong-addr
```

client：

```bash
./build/rdma-rc-client --server 127.0.0.1 --port 18519 --device rxe0 --ib-port 1 --gid-index 1 --wrong-addr
```

预期：

```text
client_wrong_addr_cqe cqe_wr_id=5000 status=remote access error
WRONG_ADDR_BOUNDARY_PASS
cleanup=complete result=pass
```

### skip-recv / RNR

server：

```bash
./build/rdma-rc-server --listen 127.0.0.1 --port 18517 --device rxe0 --ib-port 1 --gid-index 1 --skip-recv
```

client：

```bash
./build/rdma-rc-client --server 127.0.0.1 --port 18517 --device rxe0 --ib-port 1 --gid-index 1 --skip-recv
```

预期：

```text
client_skip_recv_cqe cqe_wr_id=1001 status=RNR retry counter exceeded
SKIP_RECV_BOUNDARY_PASS
cleanup=complete result=pass
```

### disconnect-after-rts

server：

```bash
./build/rdma-rc-server --listen 127.0.0.1 --port 18518 --device rxe0 --ib-port 1 --gid-index 1 --disconnect-after-rts
```

client：

```bash
./build/rdma-rc-client --server 127.0.0.1 --port 18518 --device rxe0 --ib-port 1 --gid-index 1 --disconnect-after-rts
```

预期：

```text
DISCONNECT_AFTER_RTS_PASS
cleanup=complete result=pass
```

## 8. 134 环境补齐记录

134 初始缺少 verbs 工具和头文件：

```bash
command -v ibv_devices
command -v ibv_devinfo
ls /usr/include/infiniband/verbs.h
```

`apt-get update` 首次失败，因为 Ubuntu mantic 已 EOL。处理：

```bash
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak-20260711-rdma
sudo sed -i \
  's#http://cn.archive.ubuntu.com/ubuntu/#http://old-releases.ubuntu.com/ubuntu/#g; s#http://security.ubuntu.com/ubuntu#http://old-releases.ubuntu.com/ubuntu#g; s#http://mirrors.tuna.tsinghua.edu.cn/ubuntu#http://old-releases.ubuntu.com/ubuntu#g' \
  /etc/apt/sources.list
sudo apt-get update
sudo apt-get install -y libibverbs-dev ibverbs-utils rdma-core
```

这些包的作用：

- `rdma-core`：RDMA userspace 基础组件。
- `libibverbs-dev`：提供 `<infiniband/verbs.h>` 和 libibverbs 链接能力。
- `ibverbs-utils`：提供 `ibv_devices`、`ibv_devinfo`。

## 9. 双机 RXE 绑定

双机路径必须绑定到承载 `192.168.65.x` 的网卡 `ens33`。

135：

```bash
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens33
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

预期：

```text
GID[  1]: ::ffff:192.168.65.135, RoCE v2
```

134：

```bash
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens33
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

预期：

```text
GID[  1]: ::ffff:192.168.65.134, RoCE v2
```

## 10. 双机手工测试

135 server：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
./build/rdma-rc-server --listen 0.0.0.0 --port 18520 --device rxe0 --ib-port 1 --gid-index 1
```

134 client：

```bash
cd /home/wq/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
./build/rdma-rc-client --server 192.168.65.135 --port 18520 --device rxe0 --ib-port 1 --gid-index 1
```

预期：

```text
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

135 抓包：

```bash
sudo timeout 20 tcpdump -ni ens33 udp port 4791 -c 20
```

预期：

```text
IP 192.168.65.134.<src-port> > 192.168.65.135.4791: UDP
IP 192.168.65.135.<src-port> > 192.168.65.134.4791: UDP
```

## 11. 双机脚本化测试

135 server：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
SUDO_PASSWORD='wq123456!' make dual-server
```

134 client：

```bash
cd /home/wq/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
SERVER_IP=192.168.65.135 SUDO_PASSWORD='wq123456' make dual-client
```

脚本默认参数：

```text
RDMA_NETDEV=ens33
RDMA_DEVICE=rxe0
RDMA_IB_PORT=1
RDMA_GID_INDEX=1
TCP_PORT=18520
```

生成证据：

```text
tests/server-dual.log
tests/client-dual.log
tests/tcpdump-dual-4791.log
```

双机脚本 stdout 示例：

```text
script_config name=dual_server_capture listen=0.0.0.0 port=18520 device=rxe0 ib_port=1 netdev=ens33 gid_index=1 tcpdump=1
script_step=prepare_rxe role=server status=done
script_step=tcpdump role=server status=running pid=<pid>
script_case=dual_server status=start log=tests/server-dual.log
script_step=tcpdump role=server status=done log=tests/tcpdump-dual-4791.log
PASS: dual-machine server side RoCEv2 path
script_case=dual_server status=pass
```

## 12. 常见失败和定位

### `ibv_devices` 没有 rxe0

处理：

```bash
sudo modprobe rdma_rxe
sudo rdma link add rxe0 type rxe netdev <netdev>
```

### GID index 不对

现象：

- metadata 中 GID 全 0。
- QP 无法进入 RTS。
- CQ polling 超时。

处理：

```bash
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

确认 `--gid-index` 和当前测试网卡对应。

### wrong-rkey 没有报错

预期 wrong-rkey 必须出现非 success CQE：

```text
status=remote access error
```

如果没有错误，说明测试没有真正使用错误 `rkey`，或 provider 行为需要重新确认。
