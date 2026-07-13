# TEST_RECORD_20260713_PHASE6_RKEY_ROTATION

## 1. 目标

验证同一 buffer 注销并重新注册 MR 后，新 rkey 可正常访问，而旧 rkey 的授权已经失效。

## 2. 环境

- 主机：`192.168.65.135`
- RDMA device：`rxe0`
- netdev：`ens34`
- GID index：`1`
- provider：Soft-RoCE/RXE

## 3. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test

grep -E 'rkey_rotate|rotated_rkey|stale_rkey|RKEY_.*PASS' \
  tests/kv-client.log tests/kv-server.log
```

## 4. 关键证据

```text
kv_rkey_rotate old=0x2ba new=0x433 addr=0x567107b52000
kv_client_rotated_rkey_write_cqe cqe_wr_id=5901 status=success opcode=1 byte_len=264
kv_client_rotated_rkey_read_cqe cqe_wr_id=5902 status=success opcode=2 byte_len=264
KV_RKEY_ROTATION_ACCESS_PASS
kv_client_stale_rkey_write_cqe cqe_wr_id=5903 status=remote access error opcode=0 byte_len=0
KV_STALE_RKEY_REJECT_PASS
KV_RKEY_BOUNDARY_PASS
PASS: RDMA one-sided KV rkey rotation dynamic directory CAS atomic batch WRITE READ
```

## 5. 结论与边界

MR re-register 与 stale rkey 拒绝 PASS。buffer 地址保持不变，rkey 从 `0x2ba` 变为 `0x433`；新 rkey 的 WRITE/READ 成功，旧 rkey 返回 remote access error。旧 rkey WR 位于最后，错误后不再复用 QP。本阶段不声称完成 TCP 断线重连、QP 重建或 metadata epoch 恢复。
