# TEST_FLOW

## 1. 构建

```bash
cd linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
```

## 2. 环境检查

```bash
RDMA_DEVICE=rxe0 RDMA_NETDEV=ens34 make envcheck
```

记录 `rdma link show`、`ibv_devinfo` 和 `lscpu -e=CPU,NODE,SOCKET` 输出。RXE 用于验证 verbs 语义，不能代表 RNIC 性能。

## 3. 自动 smoke

```bash
RDMA_DEVICE=rxe0 RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test
```

脚本执行顺序：

1. 创建或刷新 RXE，解析与 `RDMA_GID_ADDR` 匹配的 GID index。
2. 启动 server，交换 QPN/PSN/GID/MR addr/rkey，QP 进入 RTS。
3. fixed-slot WRITE/READ 与 record checksum 校验。
4. 4 WR batch WRITE/READ，回收两个链尾 CQE。
5. remote fetch-and-add credit 获取、归还与最终值检查。
6. CAS holder/contender/retry 状态机与 credit 恢复。
7. 动态 key directory PUT/GET 和同 bucket 碰撞拒绝。
8. server re-register MR，新 rkey WRITE/READ 成功，旧 rkey 访问失败。
9. grep 双侧 PASS marker 和关键 CQE，输出项目完成状态。

必须同时出现：

```text
tests/kv-server.log:KV_SERVER_READY
tests/kv-server.log:KV_WRITE_PASS
tests/kv-server.log:KV_READ_PASS
tests/kv-server.log:KV_CREDIT_PASS
tests/kv-server.log:KV_BATCH_WRITE_PASS
tests/kv-server.log:KV_BATCH_READ_PASS
tests/kv-server.log:KV_REMOTE_ATOMIC_ACQUIRE_PASS
tests/kv-server.log:KV_REMOTE_ATOMIC_RETURN_PASS
tests/kv-server.log:KV_REMOTE_ATOMIC_CREDIT_PASS
tests/kv-server.log:KV_CAS_HOLDER_ACQUIRE_PASS
tests/kv-server.log:KV_CAS_CONTENDER_REJECT_PASS
tests/kv-server.log:KV_CAS_RETRY_ACQUIRE_PASS
tests/kv-server.log:KV_CAS_CONTENTION_PASS
tests/kv-server.log:KV_DYNAMIC_KEY_PUT_PASS
tests/kv-server.log:KV_DYNAMIC_KEY_GET_PASS
tests/kv-server.log:KV_DIRECTORY_COLLISION_PASS
tests/kv-server.log:KV_DYNAMIC_DIRECTORY_PASS
tests/kv-server.log:KV_RKEY_ROTATION_ACCESS_PASS
tests/kv-server.log:KV_STALE_RKEY_REJECT_PASS
tests/kv-server.log:KV_RKEY_BOUNDARY_PASS
tests/kv-client.log:KV_QP_RTS_PASS
tests/kv-client.log:KV_WRITE_PASS
tests/kv-client.log:KV_READ_PASS
tests/kv-client.log:KV_CREDIT_PASS
tests/kv-client.log:KV_BATCH_WRITE_PASS
tests/kv-client.log:KV_BATCH_READ_PASS
tests/kv-client.log:KV_REMOTE_ATOMIC_ACQUIRE_PASS
tests/kv-client.log:KV_REMOTE_ATOMIC_RETURN_PASS
tests/kv-client.log:KV_REMOTE_ATOMIC_CREDIT_PASS
tests/kv-client.log:KV_CAS_HOLDER_ACQUIRE_PASS
tests/kv-client.log:KV_CAS_CONTENDER_REJECT_PASS
tests/kv-client.log:KV_CAS_RETRY_ACQUIRE_PASS
tests/kv-client.log:KV_CAS_CONTENTION_PASS
tests/kv-client.log:KV_DYNAMIC_KEY_PUT_PASS
tests/kv-client.log:KV_DYNAMIC_KEY_GET_PASS
tests/kv-client.log:KV_DIRECTORY_COLLISION_PASS
tests/kv-client.log:KV_DYNAMIC_DIRECTORY_PASS
tests/kv-client.log:KV_RKEY_ROTATION_ACCESS_PASS
tests/kv-client.log:KV_STALE_RKEY_REJECT_PASS
tests/kv-client.log:KV_RKEY_BOUNDARY_PASS
tests/kv-client.log:ONE_SIDED_KV_PASS
tests/kv-client.log:ONE_SIDED_KV_CURRENT_ENV_COMPLETE
```

只查看关键证据：

```bash
grep -E 'KV_CAS_|kv_cas_|KV_DYNAMIC_|KV_DIRECTORY_|kv_directory_' \
  tests/kv-client.log tests/kv-server.log
grep -E 'rkey_rotate|rotated_rkey|stale_rkey|RKEY_.*PASS' \
  tests/kv-client.log tests/kv-server.log
```

## 4. 手工运行

终端 A：

```bash
./build/rdma-kv-server --listen 127.0.0.1 --port 18525 \
  --device rxe0 --ib-port 1 --gid-index 1
```

终端 B：

```bash
./build/rdma-kv-client --server 127.0.0.1 --port 18525 \
  --device rxe0 --ib-port 1 --gid-index 1
```

## 5. 当前测试边界

- 当前覆盖单 client、单 QP、slot 2 单 record 与 slot 3-6 的 4 WR batch。
- Phase 2 的 credit 来自 TCP 控制面；Phase 3-4 使用 MR 尾部 remote atomic counter。
- Phase 4 是一条 QP 上两个逻辑竞争者的 CAS 状态机验证，不声称真实双 client。
- Phase 5 是 8 bucket 单槽目录；碰撞会拒绝，不包含开放寻址或删除。
- Phase 6 证明 re-register 后 stale rkey 失效，不包含 TCP 重连或 QP 重建。
- 旧 rkey 是最后一个 WR；错误 CQE 后不继续复用 QP。
- TCP ACK 只用于同步，不说明 server 收到 one-sided completion。
- 双机、真实 RNIC、跨 NUMA、双 client 和 multi-QP 均未声称完成。

## 6. 135 远端完整验证

从开发机同步后，在 135 执行：

```bash
ssh -o BatchMode=yes wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make clean
make
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test
```

最终必须出现：

```text
PASS: RDMA one-sided KV rkey rotation dynamic directory CAS atomic batch WRITE READ
project_status=ONE_SIDED_KV_CURRENT_ENV_COMPLETE
script_summary name=kv_smoke_test status=pass
```
