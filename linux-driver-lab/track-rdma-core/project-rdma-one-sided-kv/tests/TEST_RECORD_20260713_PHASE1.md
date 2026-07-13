# TEST_RECORD_20260713_PHASE1

## 1. 目标

验证固定槽位 one-sided KV 第一阶段：client 对 server MR 的 slot 2 执行 RDMA WRITE，server 校验 record；client 再对同一槽位执行 RDMA READ 并校验内容；最后以错误 rkey 验证远端访问授权边界。

## 2. 测试环境

```text
host: 192.168.65.135
kernel: 6.8.0-124-generic
RDMA device: rxe0
transport: Soft-RoCE / RXE
netdev: ens34
GID: fe80::34 (index 1)
CPU topology: CPUs 0-7, only NUMA node 0
```

RXE 用于验证 verbs、MR、rkey 和 CQE 语义；本记录不代表真实 RNIC 性能、双机时延或跨 NUMA 结论。

## 3. 编译命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
```

结果：`rdma-kv-server` 与 `rdma-kv-client` 均使用 `-Wall -Wextra -Wpedantic` 编译成功。

## 4. 自动 smoke 命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test
```

脚本执行：刷新 `rxe0`，启动 loopback server/client，执行 slot 2 WRITE/READ/错误 rkey，并检查 server/client 两侧 marker。

## 5. 关键结果

```text
tests/kv-server.log:KV_SERVER_READY
tests/kv-server.log:kv_write_record slot=2 key=client-key-2 value=client-value-write-read-slot-2 version=1 checksum=0x8a754dca
tests/kv-server.log:KV_WRITE_PASS
tests/kv-server.log:KV_READ_PASS
tests/kv-server.log:KV_RKEY_BOUNDARY_PASS
tests/kv-server.log:ONE_SIDED_KV_PASS

tests/kv-client.log:KV_QP_RTS_PASS
tests/kv-client.log:kv_client_write_cqe cqe_wr_id=1001 status=success opcode=1 byte_len=264
tests/kv-client.log:KV_WRITE_PASS
tests/kv-client.log:kv_client_read_cqe cqe_wr_id=1002 status=success opcode=2 byte_len=264
tests/kv-client.log:kv_read_record slot=2 key=client-key-2 value=client-value-write-read-slot-2 version=1 checksum=0x8a754dca
tests/kv-client.log:KV_READ_PASS
tests/kv-client.log:kv_client_bad_rkey_cqe cqe_wr_id=1003 status=remote access error opcode=0 byte_len=0
tests/kv-client.log:KV_RKEY_BOUNDARY_PASS
tests/kv-client.log:ONE_SIDED_KV_PASS

PASS: RDMA one-sided fixed-slot KV WRITE READ wrong-rkey
script_summary name=kv_smoke_test status=pass
```

## 6. 结论

```text
PASS_FIXED_SLOT_KV_RXE_135
```

- server MR 内 slot 2 被 client 的 RDMA WRITE 正确覆盖。
- client 的 RDMA READ 回读了相同的 264 字节 record。
- 错误 rkey 得到 `remote access error`，没有被误判为成功。
- server 不依赖 one-sided CQE；它仅在 TCP ACK 后检查已被远端改写的 MR。

## 7. 下一阶段

在不改变固定槽位语义的前提下，增加 credit 和多个 outstanding WRITE；之后再引入多 QP、动态 key directory 与 reconnect。真实 RNIC、双机和多 NUMA 验证仍依赖额外硬件环境。
