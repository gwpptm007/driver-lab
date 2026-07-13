# TEST_RECORD_20260713_PHASE4_CAS_CONTENTION

## 1. 目标

验证 remote compare-and-swap credit 状态机：holder 获取成功、逻辑 contender 在 credit 为 0 时被拒绝、holder 归还、contender 重试成功，最终 counter 恢复为 4。

## 2. 环境

- 主机：`192.168.65.135`
- RDMA device：`rxe0`
- netdev：`ens34`
- GID address：`fe80::34`
- GID index：`1`
- 拓扑：单机 RXE、单 client、单 RC QP

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test

grep -E 'KV_CAS_|kv_cas_|cas_.*cqe' \
  tests/kv-client.log tests/kv-server.log
```

## 4. 关键证据

```text
kv_client_cas_holder_cqe cqe_wr_id=3901 status=success opcode=3 byte_len=8
kv_cas_holder old=4 compare=4 swap=0
kv_client_cas_contender_cqe cqe_wr_id=3902 status=success opcode=3 byte_len=8
kv_cas_contender old=0 compare=4 swap=0
kv_client_cas_retry_cqe cqe_wr_id=3904 status=success opcode=3 byte_len=8
kv_cas_retry old=4 compare=4 swap=0
kv_cas_credit_final=4
KV_CAS_CONTENTION_PASS
```

## 5. 结论与边界

状态机 PASS。CAS WR 的 CQE success 不代表业务获取成功，必须检查返回旧值；contender 返回 0 且 counter 保持 0，证明失败不会覆盖 holder。该测试使用一条 QP 上的两个逻辑竞争者，不声称完成真实双 client 或多 QP 并发。
