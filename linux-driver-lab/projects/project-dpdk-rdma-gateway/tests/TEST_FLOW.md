# TEST_FLOW

## 1. Phase 1 目标

在引入 libdpdk/libibverbs 前，独立验证跨线程 descriptor、远端 wire header、SPSC ring 与 staging slot 生命周期，降低后续集成时的定位复杂度。

## 2. 环境检查

```bash
cc --version
make --version
uname -a
```

Phase 1 只需要支持 C11 atomics 的 C 编译器。

## 3. 构建

```bash
cd linux-driver-lab/projects/project-dpdk-rdma-gateway
make clean
make
```

编译参数包含：

```text
-std=c11 -Wall -Wextra -Werror
```

## 4. 自动测试

```bash
make test
```

脚本执行以下断言：

1. `gateway_request` 固定 32 字节，staging slot 64 字节对齐。
2. 40 字节 wire header 大端 roundtrip 保持业务字段一致。
3. 6 类非法 header/长度/opcode 被拒绝。
4. SPSC ring 空、满和 256 request 回绕顺序正确。
5. slot 非法重复 prepare 与 stale generation completion 被拒绝。

## 5. Marker

```text
GATEWAY_CONTRACT_LAYOUT_PASS request_size=32 wire_size=40 align=64
GATEWAY_WIRE_ROUNDTRIP_PASS
GATEWAY_WIRE_BOUNDARY_PASS cases=6
GATEWAY_RING_SPSC_PASS requests=256 full_empty_wrap=pass
GATEWAY_SLOT_LIFECYCLE_PASS stale_completion=blocked generation=2
DPDK_RDMA_GATEWAY_PHASE1_CONTRACT_PASS
```

## 6. 135 完整命令

```bash
ssh -o BatchMode=yes wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/projects/project-dpdk-rdma-gateway
chmod +x tests/*.sh
bash -n tests/*.sh
make clean
make
make test
```

## 7. 结果记录

首次实测已写入 `TEST_RECORD_20260713_PHASE1_CONTRACT.md`。Phase 2 不覆盖本记录，而是新增 pcap ingress 独立记录。

## 8. Phase 2 pcap ingress

```bash
make test-phase2
```

脚本生成 64 包 pcap，并通过 `net_pcap0` 输入应用：

```text
GATEWAY_PCAP_GENERATED packets=64 udp=48 unsupported=16
GATEWAY_INGRESS_RESULT rx=64 udp=48 unsupported=16 malformed=0 staged=48 ring_full=0 slot_exhausted=0
GATEWAY_MOCK_RDMA_RESULT dequeued=48 completed=48 payload_bytes=1536
DPDK_RDMA_GATEWAY_PHASE2_INGRESS_PASS
cleanup=complete result=pass
```

完整命令：

```bash
python3 tools/gen_gateway_pcap.py tests/runtime/gateway_phase2.pcap 64
make dpdk
./build/gateway-dpdk-ingress \
  -l 0 -n 4 --no-pci --no-huge \
  --file-prefix dpdk_rdma_gateway_phase2 \
  --vdev "net_pcap0,rx_pcap=tests/runtime/gateway_phase2.pcap" \
  -- --expected-packets 64
```

Phase 1/2 联合回归：

```bash
make test-all
```

实测记录见 `TEST_RECORD_20260713_PHASE2_INGRESS.md`。

## 9. Phase 3 RXE backend

前置检查：

```bash
rdma link show
ibv_devices
```

执行：

```bash
make test-phase3
```

脚本在本机启动 server/client 两个进程，TCP 端口默认为 18615，RDMA device 默认为 `rxe0`、GID index 默认为 1。可通过 `RDMA_DEVICE`、`RDMA_GID_INDEX`、`RDMA_PORT` 覆盖。`rdma link ACTIVE` 之外还必须确认目标 GID 非零。

关键结果：

```text
GATEWAY_RDMA_QP_RTS_PASS role=server
GATEWAY_RDMA_SERVER_READY
GATEWAY_RDMA_QP_RTS_PASS role=client
gateway_rdma_write_cqe cqe_wr_id=3001 status=success opcode=1 byte_len=72
GATEWAY_RDMA_WRITE_CQE_PASS request_id=3001 bytes=72
GATEWAY_RDMA_SLOT_COMPLETE_PASS slot=3 generation=1
GATEWAY_REMOTE_RECORD_PASS request_id=3001 payload_bytes=32
DPDK_RDMA_GATEWAY_PHASE3_SERVER_PASS
DPDK_RDMA_GATEWAY_PHASE3_RDMA_PASS
```

Phase 1-3 联合回归：

```bash
make test-all
```

完整记录见 `TEST_RECORD_20260713_PHASE3_RDMA.md`。

## 10. RXE/GID 可重复准备

Phase 3/4 默认只检查现有 RXE，不修改系统。测试机上的临时 IPv6 地址可能被网络管理服务回收，需要重建时显式执行：

```bash
SUDO_PASSWORD='<password>' GATEWAY_PREPARE_RXE=1 make test-phase4
```

`tests/rdma_test_env.sh` 将执行 `modprobe rdma_rxe`、为 `ens34` 配置 `fe80::34/64`、重建 `rxe0`，并轮询确认 `GID[1]`。可通过 `GATEWAY_RDMA_NETDEV`、`GATEWAY_RDMA_GID_ADDR`、`RDMA_DEVICE` 和 `RDMA_GID_INDEX` 覆盖。密码只通过环境传入，不写入仓库。

## 11. Phase 4 integrated path

单阶段执行：

```bash
make test-phase4
```

脚本生成 64 包 pcap，启动 integrated server，再在同一 client 进程中启动 DPDK producer 与 RDMA worker。关键结果：

```text
GATEWAY_E2E_INGRESS_RESULT rx=64 udp=48 unsupported=16 staged=48 ring_full=0 slot_exhausted=0
GATEWAY_E2E_RDMA_RESULT dequeued=48 completed=48 payload_bytes=1536 write_bytes=3456 errors=0 active_slots=0
GATEWAY_E2E_REMOTE_LAST_RECORD_PASS request_id=48 payload=GATEWAY_UDP_0062
DPDK_RDMA_GATEWAY_PHASE4_E2E_PASS
```

阶段收口 clean regression：

```bash
make clean
SUDO_PASSWORD='<password>' GATEWAY_PREPARE_RXE=1 make test-all
```

联合回归最终 marker：`DPDK_RDMA_GATEWAY_CURRENT_ENV_COMPLETE`。

完整记录见 `TEST_RECORD_20260713_PHASE4_E2E.md`。
