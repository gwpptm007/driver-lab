# TEST_RECORD_20260713_PHASE3_REMOTE_ATOMIC

## 1. 目标

验证 remote atomic credit：server MR 尾部的 8 字节 counter 初始为 4；client 使用 `IBV_WR_ATOMIC_FETCH_AND_ADD` 原子减 4 后执行 batch，结束后原子加 4 归还。验证 atomic CQE、返回旧值和 server 最终 counter。

## 2. 环境与能力

```text
host: 192.168.65.135
kernel: 6.8.0-124-generic
device: rxe0 (Soft-RoCE/RXE)
atomic_cap: ATOMIC_HCA
max_qp_rd_atom: 128
max_qp_init_rd_atom: 128
NUMA: node0 only
```

## 3. 编译与测试命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test
```

## 4. 关键结果

```text
tests/kv-server.log:kv_atomic_credit addr=0x... initial=4
tests/kv-server.log:KV_REMOTE_ATOMIC_ACQUIRE_PASS
tests/kv-server.log:kv_atomic_credit_final=4
tests/kv-server.log:KV_REMOTE_ATOMIC_RETURN_PASS
tests/kv-server.log:KV_REMOTE_ATOMIC_CREDIT_PASS

tests/kv-client.log:kv_client_atomic_acquire_cqe cqe_wr_id=1901 status=success opcode=4 byte_len=8
tests/kv-client.log:kv_atomic_acquire old=4 add=-4
tests/kv-client.log:KV_REMOTE_ATOMIC_ACQUIRE_PASS
tests/kv-client.log:kv_client_atomic_return_cqe cqe_wr_id=2901 status=success opcode=4 byte_len=8
tests/kv-client.log:kv_atomic_return old=0 add=4
tests/kv-client.log:KV_REMOTE_ATOMIC_RETURN_PASS
tests/kv-client.log:KV_REMOTE_ATOMIC_CREDIT_PASS
tests/kv-client.log:ONE_SIDED_KV_PASS
```

## 5. 结论

```text
PASS_REMOTE_ATOMIC_CREDIT_RXE_135
```

- acquire 返回旧值 4，server counter 变为 0。
- batch WRITE/READ 在 credit 持有期间完成。
- return 返回旧值 0，server counter 恢复为 4。
- Phase 1/2 和 wrong-rkey 测试继续通过。

## 6. 边界

当前是单 client、单 QP，尚未覆盖竞争失败。多个 client 直接 fetch-add `-4` 可能导致无符号下溢，下一阶段必须设计 compare-and-swap、回滚或重试状态机。RXE 结果仍不能代表真实 RNIC atomic 性能。
