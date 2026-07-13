# RDMA RC Client/Server 工程化实施计划

> **给后续执行者的要求：** 这个计划需要按任务逐步执行。每个任务用 checkbox (`- [ ]`) 跟踪进度；实现时建议使用 `superpowers:subagent-driven-development` 或 `superpowers:executing-plans` 逐项推进。

**目标：** 新建 `project-rdma-rc-client-server`，实现一个双进程 RDMA RC 小项目：TCP 负责控制面元数据交换，RDMA RC 负责 SEND/RECV 和 one-sided READ/WRITE 数据面验证。

**架构：** 项目拆成 `rdma-rc-server` 和 `rdma-rc-client` 两个独立进程。TCP 只负责交换 `gid/qpn/psn/address/rkey`，真正的数据移动通过 RC Queue Pair、Work Request 和 Completion Queue 验证。第一版先在 `192.168.65.135` 单机 Soft-RoCE 环境跑通，后续再迁移到双机 RoCEv2。

**技术栈：** C、`libibverbs`、POSIX socket、Makefile、Bash 测试脚本、Soft-RoCE `rxe0`、Ubuntu 测试机 `192.168.65.135`。

---

## 当前执行状态

```text
PASS_SINGLE_HOST
```

已在 `192.168.65.135` 单机 Soft-RoCE 上完成第一版：

- TCP 控制面交换 `gid/qpn/psn/address/rkey`。
- server/client 分别创建 context、PD、MR、CQ、RC QP。
- QP 完成 `RESET -> INIT -> RTR -> RTS`。
- RC SEND/RECV 通过。
- RDMA WRITE 通过。
- RDMA READ 通过。
- wrong-rkey 返回 `remote access error`，边界验证通过。

证据：

```text
../project-rdma-rc-client-server/tests/TEST_RECORD_20260711.md
```

## 目标目录结构

```text
linux-driver-lab/track-rdma-core/project-rdma-rc-client-server/
├── .gitignore
├── Makefile
├── README.md
├── include/
│   └── rdma_cs.h
├── src/
│   ├── common.c
│   ├── control_plane.c
│   ├── rdma_context.c
│   ├── server.c
│   └── client.c
├── docs/
│   ├── ARCHITECTURE.md
│   └── CONTROL_AND_DATA_PLANE.md
└── tests/
    ├── client_server_test.sh
    └── TEST_RECORD_20260711.md
```

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `include/rdma_cs.h` | 公共结构体、常量、函数声明、控制面元数据格式 |
| `src/common.c` | 日志、超时辅助、参数解析辅助、错误输出 |
| `src/control_plane.c` | TCP listen/connect、按行发送和接收元数据、字段校验 |
| `src/rdma_context.c` | verbs device 打开、PD/MR/CQ/QP 创建、QP 状态迁移、WR 提交、CQ 轮询、资源清理 |
| `src/server.c` | server 生命周期：监听 TCP、创建 RDMA 资源、交换元数据、接收 SEND、暴露 MR、验证 WRITE |
| `src/client.c` | client 生命周期：连接 TCP、创建 RDMA 资源、交换元数据、SEND、WRITE、READ、错误边界 |
| `tests/client_server_test.sh` | 构建项目、准备 RXE 地址、启动 server、运行 client、断言输出、收集日志 |
| `tests/TEST_RECORD_20260711.md` | 完整测试记录：在哪里编译、执行了什么命令、预期输出、实际输出 |
| `docs/ARCHITECTURE.md` | 项目结构、对象生命周期、进程模型、Mermaid 图 |
| `docs/CONTROL_AND_DATA_PLANE.md` | TCP 元数据、RC 状态机、SEND/RECV、READ/WRITE、故障边界 |

## 第一版验收目标

```text
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
WRONG_RKEY_BOUNDARY_PASS
RESOURCE_CLEANUP_PASS
```

## Phase A：项目骨架

- [ ] 创建目标目录结构。
- [ ] 添加 `Makefile`，构建两个二进制：

```text
build/rdma-rc-server
build/rdma-rc-client
```

- [ ] 添加可以编译通过的源码文件：

```text
src/common.c
src/control_plane.c
src/rdma_context.c
src/server.c
src/client.c
```

- [ ] 在 `include/rdma_cs.h` 中定义公共常量：

```c
#define RDMA_CS_DEFAULT_PORT "18515"
#define RDMA_CS_DEFAULT_DEVICE "rxe0"
#define RDMA_CS_DEFAULT_IB_PORT 1
#define RDMA_CS_DEFAULT_GID_INDEX 1
#define RDMA_CS_BUFFER_SIZE 4096
#define RDMA_CS_CQ_TIMEOUT_MS 5000
```

- [ ] 在测试机执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
make clean
make
```

预期结果：

```text
build/rdma-rc-server
build/rdma-rc-client
```

## Phase B：TCP 控制面

- [ ] server 监听 `127.0.0.1:18515`。
- [ ] client 连接 `127.0.0.1:18515`。
- [ ] 双方各发送一行本地元数据。
- [ ] 使用下面的行格式：

```text
role=server qpn=123 psn=0x111111 gid_index=1 gid=00000000000000000000000000000000 addr=0x12345678 rkey=0xabcdef01
```

- [ ] 校验必须存在的字段：

```text
role
qpn
psn
gid_index
gid
addr
rkey
```

- [ ] 添加控制面单测模式：

```bash
./build/rdma-rc-server --control-plane-only --listen 127.0.0.1 --port 18515
./build/rdma-rc-client --control-plane-only --server 127.0.0.1 --port 18515
```

预期标记：

```text
server_control_plane=pass
client_control_plane=pass
TCP_CONTROL_PLANE_PASS
```

## Phase C：RDMA 资源生命周期

- [ ] 打开 verbs device `rxe0`。
- [ ] 查询 IB port `1`。
- [ ] 查询 GID index `1`。
- [ ] 分配 PD。
- [ ] 注册一段本地 MR，权限包含：

```text
IBV_ACCESS_LOCAL_WRITE
IBV_ACCESS_REMOTE_READ
IBV_ACCESS_REMOTE_WRITE
```

- [ ] 创建 CQ。
- [ ] 创建 RC QP。
- [ ] 打印本地元数据：

```text
local_qpn=<qpn>
local_psn=<psn>
local_gid=<gid>
local_addr=<addr>
local_rkey=<rkey>
```

- [ ] 执行 dry-run：

```bash
./build/rdma-rc-server --dry-run
./build/rdma-rc-client --dry-run
```

预期结果：

```text
rdma_resources=created
cleanup=complete
```

## Phase D：RC QP 状态迁移

- [ ] TCP 元数据交换完成后，server QP 完成：

```text
RESET -> INIT -> RTR -> RTS
```

- [ ] TCP 元数据交换完成后，client QP 完成：

```text
RESET -> INIT -> RTR -> RTS
```

- [ ] 使用 TCP 控制面拿到的远端 `qpn/psn/gid`。
- [ ] RoCE 路径使用 GRH，全局地址字段设置 `is_global=1`，`sgid_index` 使用本地有效 GID index。
- [ ] 打印：

```text
server_qp_state=RTS
client_qp_state=RTS
RC_QP_RTS_PASS
```

## Phase E：RC SEND/RECV

- [ ] server 先 post 一个 Receive WR。
- [ ] client post 一个 signaled Send WR，payload 为：

```text
hello-from-client-send
```

- [ ] client 轮询 send CQE。
- [ ] server 轮询 receive CQE。
- [ ] server 校验收到的 payload。
- [ ] 打印：

```text
send_cqe_status=success
recv_cqe_status=success
server_recv_payload=hello-from-client-send
RC_SEND_RECV_PASS
```

## Phase F：RDMA WRITE

- [ ] server 通过 TCP 元数据暴露 `addr/rkey`。
- [ ] client post 一个 signaled RDMA WRITE，写入 server MR，payload 为：

```text
written-by-client-rdma-write
```

- [ ] client 轮询本地 CQE。
- [ ] client 通过 TCP 通知 server 写完成。
- [ ] server 校验自己的 MR 内容已经改变。
- [ ] 打印：

```text
write_cqe_status=success
server_write_payload=written-by-client-rdma-write
RDMA_WRITE_PASS
```

## Phase G：RDMA READ

- [ ] server 准备 MR 内容：

```text
read-from-server-rdma-read
```

- [ ] client post 一个 signaled RDMA READ，把 server MR 内容读到 client 本地 MR。
- [ ] client 轮询本地 CQE。
- [ ] client 校验本地 payload。
- [ ] 打印：

```text
read_cqe_status=success
client_read_payload=read-from-server-rdma-read
RDMA_READ_PASS
```

## Phase H：错误 rkey 边界

- [ ] 给 client 增加参数：

```text
--wrong-rkey
```

- [ ] 在该模式下，client 在 RDMA WRITE 前把 server `rkey` 翻转一位。
- [ ] 轮询 CQ，预期拿到非 success completion，或者 provider 在提交/完成阶段返回错误。
- [ ] 只有检测到错误 rkey 被拒绝时，测试才算成功。
- [ ] 打印：

```text
wrong_rkey_detected=pass
WRONG_RKEY_BOUNDARY_PASS
```

## Phase I：自动化测试和测试记录

- [ ] 实现 `tests/client_server_test.sh`。
- [ ] 脚本先恢复 Soft-RoCE 测试所需的 IPv6 link-local 地址：

```bash
sudo ip -6 addr add fe80::34/64 dev ens34 2>/dev/null || true
while ip -6 addr show ens34 | grep -q tentative; do sleep 1; done
```

- [ ] 脚本构建项目：

```bash
make clean
make
```

- [ ] 脚本后台启动 server：

```bash
./build/rdma-rc-server --listen 127.0.0.1 --port 18515 > tests/server.log 2>&1 &
```

- [ ] 脚本运行 client：

```bash
./build/rdma-rc-client --server 127.0.0.1 --port 18515 > tests/client.log 2>&1
```

- [ ] 脚本断言下面的标记全部存在：

```text
TCP_CONTROL_PLANE_PASS
RC_QP_RTS_PASS
RC_SEND_RECV_PASS
RDMA_WRITE_PASS
RDMA_READ_PASS
RESOURCE_CLEANUP_PASS
```

- [ ] 脚本执行 wrong-rkey 模式，并断言：

```text
WRONG_RKEY_BOUNDARY_PASS
```

- [ ] 更新 `tests/TEST_RECORD_20260711.md`，记录完整命令：

```text
test_machine=192.168.65.135
repo=/home/wq7/workspace/driver-lab
project=linux-driver-lab/track-rdma-core/project-rdma-rc-client-server
build_command=make clean && make
test_command=bash tests/client_server_test.sh
result=PASS
```

## Phase J：文档

- [ ] 编写 `docs/ARCHITECTURE.md`，至少包含这些 Mermaid 图：

```text
server/client 进程拓扑
verbs 对象归属关系
资源清理顺序
```

- [ ] 编写 `docs/CONTROL_AND_DATA_PLANE.md`，至少包含这些 Mermaid 图：

```text
TCP 元数据交换
QP RESET/INIT/RTR/RTS 状态迁移
SEND/RECV 时序
RDMA WRITE 时序
RDMA READ 时序
错误 rkey 路径
```

- [ ] 更新 `README.md`，说明：

```text
这个项目学什么
怎么编译
怎么运行
怎么看日志
Soft-RoCE 能证明什么
哪些结论仍然需要真实 RNIC 或双机拓扑
```

## Phase K：双机 RoCEv2 后续扩展

这不是第一版单机验收范围，但代码需要为双机保留入口。

- [ ] 增加 CLI 参数：

```text
--listen <ip>
--server <ip>
--port <port>
--device <verbs-device>
--gid-index <index>
--ib-port <port>
```

- [ ] 第二台测试机准备好后，先验证：

```bash
rdma link
ibv_devinfo
ip addr
ip route
tcpdump -ni <netdev> udp port 4791
```

- [ ] 双机验收标记：

```text
DUAL_HOST_TCP_PASS
DUAL_HOST_ROCEV2_PACKET_SEEN
DUAL_HOST_RC_SEND_RECV_PASS
DUAL_HOST_RDMA_WRITE_READ_PASS
```

## 推荐执行顺序

1. Phase A：项目骨架和构建。
2. Phase B：只跑 TCP 控制面。
3. Phase C：RDMA 资源生命周期。
4. Phase D：QP 进入 RTS。
5. Phase E：SEND/RECV。
6. Phase F：RDMA WRITE。
7. Phase G：RDMA READ。
8. Phase H：错误 rkey 边界。
9. Phase I：自动测试和测试记录。
10. Phase J：文档。
11. Phase K：第二台机器准备好后再做双机扩展。

## 停止条件

- 如果 `rxe0` 不存在，先修复 Phase 1 环境，不要急着改代码。
- 如果 `ens34` 没有稳定的 `fe80::34/64`，先恢复地址，再排查 QP 状态迁移。
- 如果 QP 在 RTR 失败，先检查 GID index 和 IPv6 地址是否 still tentative，再改 verbs 逻辑。
- 如果 CQ 轮询超时，必须记录超时发生在 SEND、RECV、WRITE、READ 还是 wrong-rkey 模式。
