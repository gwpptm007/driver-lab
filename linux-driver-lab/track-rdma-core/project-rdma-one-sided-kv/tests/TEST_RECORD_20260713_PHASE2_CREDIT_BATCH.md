# TEST_RECORD_20260713_PHASE2_CREDIT_BATCH

## 1. 目标

在 Phase 1 的 fixed-slot 单 record WRITE/READ 之后，验证 server 授予 4 个应用层 credit，client 一次 `ibv_post_send()` 链式提交 4 个 RDMA WRITE，再链式提交 4 个 RDMA READ。每个 WR 使用已注册 client MR 中独立的 record 区域，只有链尾 WR 请求 CQE。

## 2. 环境

```text
host: 192.168.65.135
kernel: 6.8.0-124-generic
RDMA device: rxe0
transport: Soft-RoCE / RXE
netdev: ens34
GID: fe80::34 (index 1)
NUMA: node0 only
```

这次仍是 RXE 语义和脚本闭环验证，不是 RNIC 吞吐、真实网络时延或多 NUMA 性能结果。

## 3. 编译与测试命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make

RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test
```

脚本刷新 `rxe0`，保留 Phase 1 的 slot 2 单 record 验证，然后执行 credit=4 的 slot 3-6 链式 WRITE/READ，最后执行错误 rkey 边界。

## 4. 关键 marker

```text
tests/kv-server.log:KV_CREDIT_GRANTED count=4
tests/kv-server.log:KV_CREDIT_PASS
tests/kv-server.log:kv_batch_write first_slot=3 count=4 tail_key=client-credit-key-6
tests/kv-server.log:KV_BATCH_WRITE_PASS
tests/kv-server.log:KV_BATCH_READ_PASS

tests/kv-client.log:KV_CREDIT_PASS
tests/kv-client.log:kv_client_batch_write_tail_cqe cqe_wr_id=2004 status=success opcode=1 byte_len=264
tests/kv-client.log:KV_BATCH_WRITE_PASS
tests/kv-client.log:kv_client_batch_read_tail_cqe cqe_wr_id=2008 status=success opcode=2 byte_len=264
tests/kv-client.log:kv_batch_read first_slot=3 count=4 tail_key=client-credit-key-6
tests/kv-client.log:KV_BATCH_READ_PASS

tests/kv-client.log:KV_RKEY_BOUNDARY_PASS
tests/kv-client.log:ONE_SIDED_KV_PASS
PASS: RDMA one-sided fixed-slot KV credit batch WRITE READ wrong-rkey
script_summary name=kv_smoke_test status=pass
```

## 5. 结论

```text
PASS_CREDIT_BATCH_KV_RXE_135
```

- 4 个 credit 约束 batch 大小为 4，覆盖 slot 3-6。
- 一次 `ibv_post_send()` 提交 4 个 WRITE WR，链尾 `wr_id=2004` 成功；server 校验全部 record。
- 一次 `ibv_post_send()` 提交 4 个 READ WR，链尾 `wr_id=2008` 成功；client 回读并比较全部 record。
- 单 record Phase 1 和错误 rkey 边界仍保持通过。

## 6. 边界和下一步

credit 由 TCP 控制面授予，尚未使用 RDMA atomic 操作；当前只有单 client、单 QP。下一阶段候选是 remote atomic credit 或多 QP，二者不应在没有并发语义设计的情况下同时引入。
