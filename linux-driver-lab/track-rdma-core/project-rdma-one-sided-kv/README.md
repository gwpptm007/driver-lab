# project-rdma-one-sided-kv

本项目把 RC client/server 的 one-sided READ/WRITE 演示推进为一个可测试的 KV 原型。server 注册 record、key directory 和 atomic credit counter 组成的 MR；client 通过 TCP 控制面获得 QP 信息与 `addr/rkey`，KV payload、目录访问和 credit 操作均使用 one-sided verbs。

当前状态：`ONE_SIDED_KV_CURRENT_ENV_COMPLETE`。2026-07-13 已在 `192.168.65.135` 的 Soft-RoCE/RXE 环境完成 Phase 1-6，并保留可复现脚本和逐阶段测试记录。

## 已完成范围

| Phase | 能力 | 证据状态 |
|---|---|---|
| 1 | 固定 slot 完整 record WRITE/READ、checksum | PASS |
| 2 | 4 credit、4 WR 链式 batch、仅尾 WR signaled | PASS |
| 3 | remote fetch-and-add 获取/归还 credit | PASS |
| 4 | CAS 竞争拒绝、归还、重试和 counter 恢复 | PASS |
| 5 | FNV-1a 动态 key directory、PUT/GET、碰撞拒绝 | PASS |
| 6 | MR re-register、new rkey 成功、stale rkey 失效 | PASS |

Phase 4 在同一条 QP 上模拟两个逻辑竞争者，验证 CAS 状态机和失败不改值语义；它不是双 client、多 QP 证据。当前环境范围不包含双机、真实双 client、多 QP、故障重连、真实 RNIC 性能和跨 NUMA 性能结论。

## 构建

```bash
cd linux-driver-lab/track-rdma-core/project-rdma-one-sided-kv
make
```

## 单机 RXE smoke

```bash
RDMA_NETDEV=ens34 RDMA_GID_ADDR=fe80::34 \
SUDO_PASSWORD='<sudo-password>' make test
```

成功时输出：

```text
KV_WRITE_PASS
KV_READ_PASS
KV_CREDIT_PASS
KV_BATCH_WRITE_PASS
KV_BATCH_READ_PASS
KV_REMOTE_ATOMIC_CREDIT_PASS
KV_CAS_CONTENTION_PASS
KV_DYNAMIC_DIRECTORY_PASS
KV_RKEY_ROTATION_ACCESS_PASS
KV_STALE_RKEY_REJECT_PASS
KV_RKEY_BOUNDARY_PASS
ONE_SIDED_KV_PASS
ONE_SIDED_KV_CURRENT_ENV_COMPLETE
PASS: RDMA one-sided KV rkey rotation dynamic directory CAS atomic batch WRITE READ
```

完整流程、日志 marker 和手工 server/client 命令见 `tests/TEST_FLOW.md`；原理图见 `docs/ARCHITECTURE.md`。
