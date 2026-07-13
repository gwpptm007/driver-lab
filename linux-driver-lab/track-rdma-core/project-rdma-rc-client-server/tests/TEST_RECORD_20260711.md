# TEST_RECORD_20260711

## 1. 测试目标

本次测试验证 `project-rdma-rc-client-server` 的单机 Soft-RoCE 工程化闭环：

- 编译独立的 `rdma-rc-server` 和 `rdma-rc-client`。
- 验证 TCP 控制面可以交换 `role/qpn/psn/gid/addr/rkey`。
- 重新准备 `rdma_rxe` / `rxe0`。
- 验证 server/client 各自创建 RDMA resources：context、PD、MR、CQ、RC QP。
- 验证独立进程完成 QP `RESET -> INIT -> RTR -> RTS`。
- 验证 RC SEND/RECV。
- 验证 RDMA WRITE。
- 验证 RDMA READ。
- 验证 wrong-rkey remote access error。
- 验证 wrong-addr remote access error。
- 验证 skip-recv / RNR retry exceeded。
- 验证 disconnect-after-rts 后资源可以完整清理。

## 2. 测试环境

```text
host=192.168.65.135
user=wq7
repo=/home/wq7/workspace/driver-lab
project=linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
netdev=ens34
soft_roce=rxe0
gid_index=1
```

## 3. 执行命令

在测试机执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
SUDO_PASSWORD='wq123456!' make test
```

脚本内部会执行：

```bash
make clean
make

./build/rdma-rc-server --control-plane-only --listen 127.0.0.1 --port 18515
./build/rdma-rc-client --control-plane-only --server 127.0.0.1 --port 18515

sudo modprobe rdma_rxe
sudo ip -6 addr add fe80::34/64 dev ens34 2>/dev/null || true
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens34

./build/rdma-rc-server --dry-run --device rxe0 --ib-port 1 --gid-index 1
./build/rdma-rc-client --dry-run --device rxe0 --ib-port 1 --gid-index 1

./build/rdma-rc-server --listen 127.0.0.1 --port 18516 --device rxe0 --ib-port 1 --gid-index 1
./build/rdma-rc-client --server 127.0.0.1 --port 18516 --device rxe0 --ib-port 1 --gid-index 1

./build/rdma-rc-server --listen 127.0.0.1 --port 18519 --device rxe0 --ib-port 1 --gid-index 1 --wrong-addr
./build/rdma-rc-client --server 127.0.0.1 --port 18519 --device rxe0 --ib-port 1 --gid-index 1 --wrong-addr

./build/rdma-rc-server --listen 127.0.0.1 --port 18517 --device rxe0 --ib-port 1 --gid-index 1 --skip-recv
./build/rdma-rc-client --server 127.0.0.1 --port 18517 --device rxe0 --ib-port 1 --gid-index 1 --skip-recv

./build/rdma-rc-server --listen 127.0.0.1 --port 18518 --device rxe0 --ib-port 1 --gid-index 1 --disconnect-after-rts
./build/rdma-rc-client --server 127.0.0.1 --port 18518 --device rxe0 --ib-port 1 --gid-index 1 --disconnect-after-rts
```

## 4. 总体结果

```text
PASS: TCP control plane metadata exchange
PASS: RDMA resource lifecycle dry-run
PASS: RC client/server SEND WRITE READ wrong-rkey
PASS: wrong-addr remote address boundary
PASS: skip-recv RNR boundary
PASS: disconnect-after-rts cleanup boundary
```

结论：本次单机 Soft-RoCE 测试全部通过。

## 5. 关键日志

### 5.1 TCP 控制面

server：
```text
server_listen=127.0.0.1:18515
server_local_metadata role=server qpn=123 psn=0x111111 gid_index=1 gid=00000000000000000000000000000000 addr=0x12345678 rkey=0xabcdef01
server_remote_metadata role=client qpn=456 psn=0x222222 gid_index=1 gid=00000000000000000000000000000000 addr=0x87654321 rkey=0x10203040
server_control_plane=pass
TCP_CONTROL_PLANE_PASS
```

client：
```text
client_local_metadata role=client qpn=456 psn=0x222222 gid_index=1 gid=00000000000000000000000000000000 addr=0x87654321 rkey=0x10203040
client_remote_metadata role=server qpn=123 psn=0x111111 gid_index=1 gid=00000000000000000000000000000000 addr=0x12345678 rkey=0xabcdef01
client_control_plane=pass
TCP_CONTROL_PLANE_PASS
```

### 5.2 RDMA resource dry-run

```text
server_local_metadata role=server qpn=17 psn=0x111111 gid_index=1 gid=fe800000000000000000000000000034 addr=0x63b4ec2b1000 rkey=0x24f
rdma_resources=created
cleanup=complete result=pass

client_local_metadata role=client qpn=18 psn=0x222222 gid_index=1 gid=fe800000000000000000000000000034 addr=0x63d3c435b000 rkey=0x365
rdma_resources=created
cleanup=complete result=pass
```

### 5.3 正常数据面 + wrong-rkey

server：
```text
server_qp_state=RTS
RC_QP_RTS_PASS
server_recv_cqe cqe_wr_id=2002 status=success opcode=128 byte_len=23
server_recv_payload=hello-from-client-send
RC_SEND_RECV_PASS
server_write_payload=written-by-client-rdma-write
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

client：
```text
client_qp_state=RTS
RC_QP_RTS_PASS
client_send_cqe cqe_wr_id=1001 status=success opcode=0 byte_len=23
RC_SEND_RECV_PASS
client_write_cqe cqe_wr_id=3001 status=success opcode=1 byte_len=29
RDMA_WRITE_PASS
client_read_cqe cqe_wr_id=4001 status=success opcode=2 byte_len=27
client_read_payload=read-from-server-rdma-read
RDMA_READ_PASS
client_wrong_rkey_cqe cqe_wr_id=5001 status=remote access error opcode=0 byte_len=0
wrong_rkey_detected=pass
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

### 5.4 wrong-addr 边界

server：
```text
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_ADDR_BOUNDARY_PASS
cleanup=complete result=pass
```

client：
```text
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
client_wrong_addr_cqe cqe_wr_id=5000 status=remote access error opcode=0 byte_len=0
wrong_addr_detected=pass
WRONG_ADDR_BOUNDARY_PASS
cleanup=complete result=pass
```

### 5.5 skip-recv / RNR 边界

server：
```text
RC_QP_RTS_PASS
SKIP_RECV_BOUNDARY_PASS
cleanup=complete result=pass
```

client：
```text
RC_QP_RTS_PASS
client_skip_recv_cqe cqe_wr_id=1001 status=RNR retry counter exceeded opcode=0 byte_len=0
skip_recv_detected=pass
SKIP_RECV_BOUNDARY_PASS
cleanup=complete result=pass
```

### 5.6 disconnect-after-rts 边界

server：
```text
RC_QP_RTS_PASS
DISCONNECT_AFTER_RTS_PASS
cleanup=complete result=pass
```

client：
```text
RC_QP_RTS_PASS
DISCONNECT_AFTER_RTS_PASS
cleanup=complete result=pass
```

## 6. 学习结论

- TCP 控制面只负责交换连接和授权元数据，真正的数据不通过 TCP 搬运。
- RC QP 必须依赖对端 `qpn/psn/gid` 才能进入 RTR/RTS。
- SEND/RECV 是双边语义，接收端不 post RECV 时，发送端会看到 RNR 类 CQE 错误。
- RDMA WRITE/READ 是 one-sided 语义，完成事件主要出现在发起端 CQ。
- `rkey` 错误和 `remote_addr` 越界都会以 CQE error 体现，不是 TCP 错误。
- RTS 后提前断开控制面并不等于资源泄漏，程序必须按依赖顺序清理 QP、MR、CQ、PD 和 context。

## 7. 第二台机器环境准备

第二台机器信息：

```text
host=192.168.65.134
user=wq
password=wq123456
root_password=wq123456
```

### 7.1 初始状态

首次检查时，`192.168.65.134` 可以 SSH 登录，机器名为 `wanqi-vm`：

```text
wanqi-vm
ens33 UP 192.168.65.134/24 fe80::d70c:27d3:291f:9659/64
```

但只有 `rdma` 工具，缺少 verbs 编译和观察工具：

```text
/usr/bin/gcc
/usr/bin/make
command -v ibv_devices: empty
command -v ibv_devinfo: empty
ls /usr/include/infiniband/verbs.h: empty
```

### 7.2 修复 apt 源

`apt-get update` 首次失败：

```text
E: 仓库 “http://cn.archive.ubuntu.com/ubuntu mantic Release” 不再含有 Release 文件。
E: 仓库 “http://security.ubuntu.com/ubuntu mantic-security Release” 不再含有 Release 文件。
```

原因：Ubuntu mantic 已 EOL，普通源不再提供 Release 文件。

处理命令：

```bash
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak-20260711-rdma
sudo sed -i \
  's#http://cn.archive.ubuntu.com/ubuntu/#http://old-releases.ubuntu.com/ubuntu/#g; s#http://security.ubuntu.com/ubuntu#http://old-releases.ubuntu.com/ubuntu#g; s#http://mirrors.tuna.tsinghua.edu.cn/ubuntu#http://old-releases.ubuntu.com/ubuntu#g' \
  /etc/apt/sources.list
sudo apt-get update
sudo apt-get install -y libibverbs-dev ibverbs-utils rdma-core
```

补充包的作用：

- `rdma-core`：提供 RDMA userspace 基础组件。
- `libibverbs-dev`：提供 `<infiniband/verbs.h>` 和链接库，用于编译本项目。
- `ibverbs-utils`：提供 `ibv_devices`、`ibv_devinfo`，用于观察 provider、device、GID。

### 7.3 134 Soft-RoCE 能力

```bash
sudo modprobe rdma_rxe
sudo ip -6 addr add fe80::134/64 dev ens33 2>/dev/null || true
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens33
rdma link
ibv_devices
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

结果：

```text
link rxe0/1 state ACTIVE physical_state LINK_UP netdev ens33
device node GUID
rxe0 020c29fffe68ded0
GID[  0]: fe80::20c:29ff:fe68:ded0, RoCE v2
GID[  1]: ::ffff:192.168.65.134, RoCE v2
GID[  2]: fe80::134, RoCE v2
```

### 7.4 134 单机自测

```bash
cd /home/wq/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
RDMA_NETDEV=ens33 RDMA_GID_ADDR=fe80::134 RDMA_GID_INDEX=2 SUDO_PASSWORD='wq123456' make test
```

结果：

```text
PASS: TCP control plane metadata exchange
PASS: RDMA resource lifecycle dry-run
PASS: RC client/server SEND WRITE READ wrong-rkey
PASS: wrong-addr remote address boundary
PASS: skip-recv RNR boundary
PASS: disconnect-after-rts cleanup boundary
```

## 8. 双机 RoCEv2 验证

### 8.1 双机 RXE 绑定选择

135 的 `192.168.65.135` 在 `ens33`，不是原来单机测试用的 `ens34`：

```text
ens33 UP 192.168.65.135/24 fe80::6e4d:b76b:3e79:cf8d/64
ens34 UP fe80::135/64 fe80::34/64
```

因此双机测试把两边 `rxe0` 都绑定到 `ens33`，使用 IPv4-mapped GID index `1`：

135：
```bash
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens33
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

```text
GID[  1]: ::ffff:192.168.65.135, RoCE v2
```

134：
```bash
sudo rdma link delete rxe0 2>/dev/null || true
sudo rdma link add rxe0 type rxe netdev ens33
ibv_devinfo -d rxe0 -v | sed -n '/GID\[/,+2p' | head -20
```

```text
GID[  1]: ::ffff:192.168.65.134, RoCE v2
```

### 8.2 跨主机 RC client/server

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

server 关键结果：

```text
server_local_metadata role=server qpn=17 psn=0x111111 gid_index=1 gid=00000000000000000000ffffc0a84187
server_remote_metadata role=client qpn=17 psn=0x222222 gid_index=1 gid=00000000000000000000ffffc0a84186
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

client 关键结果：

```text
client_local_metadata role=client qpn=17 psn=0x222222 gid_index=1 gid=00000000000000000000ffffc0a84186
client_remote_metadata role=server qpn=17 psn=0x111111 gid_index=1 gid=00000000000000000000ffffc0a84187
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
client_wrong_rkey_cqe cqe_wr_id=5001 status=remote access error opcode=0 byte_len=0
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

### 8.3 UDP 4791 抓包

135 抓包：

```bash
sudo timeout 20 tcpdump -ni ens33 udp port 4791 -c 20
```

抓到 RoCEv2 UDP 4791：

```text
19:36:26.260059 IP 192.168.65.134.49476 > 192.168.65.135.4791: UDP, length 40
19:36:26.260130 IP 192.168.65.135.49476 > 192.168.65.134.4791: UDP, length 20
19:36:26.260247 IP 192.168.65.134.49476 > 192.168.65.135.4791: UDP, length 64
19:36:26.260270 IP 192.168.65.135.49476 > 192.168.65.134.4791: UDP, length 20
19:36:26.260682 IP 192.168.65.134.49476 > 192.168.65.135.4791: UDP, length 32
19:36:26.260734 IP 192.168.65.135.49476 > 192.168.65.134.4791: UDP, length 48
19:36:26.260936 IP 192.168.65.134.49476 > 192.168.65.135.4791: UDP, length 48
19:36:26.260985 IP 192.168.65.135.49476 > 192.168.65.134.4791: UDP, length 20
8 packets captured
8 packets received by filter
0 packets dropped by kernel
```

结论：双机 Soft-RoCE/RoCEv2 路径已跑通，且能在物理网络接口上观察到 UDP 4791。

## 9. 双机脚本化入口

为了把上面的手工流程固化为可重复动作，新增两个测试脚本：

```text
tests/dual_server_capture.sh
tests/dual_client_run.sh
```

Makefile 入口：

```bash
make dual-server
make dual-client
```

135 server 侧执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
SUDO_PASSWORD='wq123456!' make dual-server
```

134 client 侧执行：

```bash
cd /home/wq/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
SERVER_IP=192.168.65.135 SUDO_PASSWORD='wq123456' make dual-client
```

脚本职责：

- server 侧准备 `rxe0 -> ens33`。
- server 侧启动 `tcpdump -ni ens33 udp port 4791`。
- server 侧运行 `rdma-rc-server --listen 0.0.0.0 --gid-index 1`。
- client 侧准备 `rxe0 -> ens33`。
- client 侧运行 `rdma-rc-client --server 192.168.65.135 --gid-index 1`。
- 两侧分别 grep `TCP_CONTROL_PLANE_PASS`、`RC_QP_RTS_PASS`、`RC_SEND_RECV_PASS`、`RDMA_WRITE_PASS`、`RDMA_READ_PASS`、`WRONG_RKEY_BOUNDARY_PASS` 和 `cleanup=complete result=pass`。
- server 侧额外 grep tcpdump 日志中的 `UDP`。

输出证据：

```text
tests/server-dual.log
tests/client-dual.log
tests/tcpdump-dual-4791.log
```

### 9.1 脚本化实测结果

135 `make dual-server` 输出：

```text
server_netdev=ens33
link rxe0/1 state ACTIVE physical_state LINK_UP netdev ens33
GID[  1]: ::ffff:192.168.65.135, RoCE v2
PASS: dual-machine server side RoCEv2 path
```

134 `make dual-client` 输出：

```text
client_netdev=ens33
link rxe0/1 state ACTIVE physical_state LINK_UP netdev ens33
GID[  1]: ::ffff:192.168.65.134, RoCE v2
PASS: dual-machine client side RC data path
```

server 日志：

```text
server_listen=0.0.0.0:18520
server_local_metadata role=server qpn=17 psn=0x111111 gid_index=1 gid=00000000000000000000ffffc0a84187
server_remote_metadata role=client qpn=17 psn=0x222222 gid_index=1 gid=00000000000000000000ffffc0a84186
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

client 日志：

```text
client_local_metadata role=client qpn=17 psn=0x222222 gid_index=1 gid=00000000000000000000ffffc0a84186
client_remote_metadata role=server qpn=17 psn=0x111111 gid_index=1 gid=00000000000000000000ffffc0a84187
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
client_wrong_rkey_cqe cqe_wr_id=5001 status=remote access error opcode=0 byte_len=0
WRONG_RKEY_BOUNDARY_PASS
cleanup=complete result=pass
```

tcpdump 日志：

```text
22:37:42.197152 IP 192.168.65.134.49441 > 192.168.65.135.4791: UDP, length 40
22:37:42.197175 IP 192.168.65.135.49441 > 192.168.65.134.4791: UDP, length 20
22:37:42.197378 IP 192.168.65.134.49441 > 192.168.65.135.4791: UDP, length 64
22:37:42.197386 IP 192.168.65.135.49441 > 192.168.65.134.4791: UDP, length 20
8 packets captured
8 packets received by filter
0 packets dropped by kernel
```

## 10. 日志增强记录

本次补充了两类日志：

- 程序内部日志：`app_config`、`phase=...`
- 测试脚本日志：`script_config`、`script_step`、`script_case`、`script_summary`

### 10.1 单机脚本输出

135 执行：

```bash
SUDO_PASSWORD='wq123456!' make test
```

关键 stdout：

```text
script_config name=client_server_test device=rxe0 ib_port=1 netdev=ens34 gid_addr=fe80::34 gid_index=1
script_case=control_plane status=start server_log=tests/server.log client_log=tests/client.log
script_step=prepare_rxe status=start netdev=ens34 device=rxe0 gid_addr=fe80::34
script_case=dry_run status=pass
script_case=full_wrong_rkey status=pass
script_case=wrong_addr status=pass
script_case=skip_recv status=pass
script_case=disconnect_after_rts status=pass
script_summary name=client_server_test status=pass
```

### 10.2 程序内部阶段日志

`tests/server-full.log`：

```text
app_config role=server mode=full listen=127.0.0.1 server=127.0.0.1 port=18516 device=rxe0 ib_port=1 gid_index=1 flags=
phase=resources_create role=server status=start
phase=resources_create role=server status=done
phase=tcp_listen role=server status=start
phase=tcp_accept role=server status=waiting
phase=metadata_exchange role=server status=done
phase=qp_to_rts role=server status=start
phase=qp_to_rts role=server status=done
phase=send_recv role=server status=start
phase=send_recv role=server status=done
phase=rdma_write role=server status=start
phase=rdma_write role=server status=done
phase=rdma_read role=server status=start
phase=rdma_read role=server status=done
phase=fault_boundary role=server case=wrong_rkey status=done
```

`tests/client-full.log`：

```text
app_config role=client mode=full listen=127.0.0.1 server=127.0.0.1 port=18516 device=rxe0 ib_port=1 gid_index=1 flags=
phase=resources_create role=client status=start
phase=resources_create role=client status=done
phase=tcp_connect role=client status=start
phase=metadata_exchange role=client status=done
phase=qp_to_rts role=client status=start
phase=qp_to_rts role=client status=done
phase=send_recv role=client status=wait_ready
phase=send_recv role=client status=start
phase=send_recv role=client status=done
phase=rdma_write role=client status=start
phase=rdma_write role=client status=done
phase=rdma_read role=client status=start
phase=rdma_read role=client status=done
phase=fault_boundary role=client case=wrong_rkey status=start
phase=fault_boundary role=client case=wrong_rkey status=done
```

### 10.3 双机脚本输出

135 `make dual-server`：

```text
script_config name=dual_server_capture listen=0.0.0.0 port=18520 device=rxe0 ib_port=1 netdev=ens33 gid_index=1 tcpdump=1
script_step=prepare_rxe role=server status=start netdev=ens33 device=rxe0
script_step=prepare_rxe role=server status=done
script_step=tcpdump role=server status=start log=tests/tcpdump-dual-4791.log
script_step=tcpdump role=server status=running pid=8979
script_case=dual_server status=start log=tests/server-dual.log
script_step=tcpdump role=server status=done log=tests/tcpdump-dual-4791.log
PASS: dual-machine server side RoCEv2 path
script_case=dual_server status=pass
```

134 `make dual-client`：

```text
script_config name=dual_client_run server=192.168.65.135 port=18520 device=rxe0 ib_port=1 netdev=ens33 gid_index=1
script_step=prepare_rxe role=client status=start netdev=ens33 device=rxe0
script_step=prepare_rxe role=client status=done
script_case=dual_client status=start log=tests/client-dual.log
PASS: dual-machine client side RC data path
script_case=dual_client status=pass
```
