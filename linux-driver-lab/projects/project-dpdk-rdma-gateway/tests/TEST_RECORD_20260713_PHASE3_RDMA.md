# TEST_RECORD_20260713_PHASE3_RDMA

## 1. 目标

在 135 的 RXE 环境建立 gateway server/client RC QP，把 gateway remote record 通过真实 `IBV_WR_RDMA_WRITE` 写入 server MR，以 client CQE 和 server 内存内容双重验收。

## 2. 环境

- 主机：`192.168.65.135`
- RDMA device：`rxe0`
- RDMA link：`rxe0/1 state ACTIVE`，netdev `ens34`
- IB port：1
- GID index：1，`fe80::34`
- 控制面：TCP `127.0.0.1:18615`
- 数据面：RXE RC QP + signaled RDMA WRITE
- 编译：`-std=c11 -Wall -Wextra -Werror -libverbs`

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/projects/project-dpdk-rdma-gateway
chmod +x tests/*.sh
bash -n tests/*.sh
rdma link show
ibv_devices
ip -6 addr show dev ens34
ibv_devinfo -d rxe0 -v | grep -i 'GID\['
make clean
make test-all
```

Phase 3 单独执行：

```bash
RDMA_DEVICE=rxe0 RDMA_GID_INDEX=1 RDMA_PORT=18615 make test-phase3
```

## 4. 实测结果

Server：

```text
GATEWAY_RDMA_QP_RTS_PASS role=server
GATEWAY_RDMA_SERVER_READY
GATEWAY_REMOTE_RECORD_PASS request_id=3001 payload_bytes=32
DPDK_RDMA_GATEWAY_PHASE3_SERVER_PASS
cleanup=complete role=server result=pass
```

Client：

```text
GATEWAY_RDMA_QP_RTS_PASS role=client
gateway_rdma_write_cqe cqe_wr_id=3001 status=success opcode=1 byte_len=72
GATEWAY_RDMA_WRITE_CQE_PASS request_id=3001 bytes=72
GATEWAY_RDMA_SLOT_COMPLETE_PASS slot=3 generation=1
DPDK_RDMA_GATEWAY_PHASE3_RDMA_PASS
cleanup=complete role=client result=pass
```

## 5. 首轮问题与修正

首次运行中 RDMA WRITE/CQE 已成功，但控制面严格字符串比较失败。原因是复用的 `rdma_cs_recv_line()` 会保留行尾 `\n`。修正 server/client 按 framing 契约比较带换行 token 后通过；没有绕过或降级 RDMA 数据面。

最终联合回归前 `ens34` 的实验 IPv6 地址消失，出现“`rxe0` link ACTIVE，但目标 GID 不可用”的环境失败。按既有 RXE 流程执行以下准备后恢复：

```bash
sudo ip -6 addr replace fe80::34/64 dev ens34 nodad
sudo rdma link delete rxe0
sudo rdma link add rxe0 type rxe netdev ens34
ibv_devinfo -d rxe0 -v | grep -i 'GID\['
```

最终确认 `GID[1]=fe80::34`。测试脚本不隐式修改系统网络配置，只检查已有 RXE device；环境准备由测试记录显式保留。

## 6. 守恒与判定

```text
local record bytes = wire header 40 + payload 32 = WRITE 72
client CQE wr_id = request_id = 3001
client completed slots = 1
server validated records = 1
```

## 7. 结论与边界

Phase 3 RXE backend PASS。真实 MR、QP、RDMA WRITE 和 CQ polling 已验证，但 payload 仍由 client 构造，尚未与 Phase 2 的 pcap ingress 在同一进程内串联。RXE 结果不能表述为真实 RNIC 性能。
