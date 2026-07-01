# RDMA RC QP State Machine

本项目创建两个本地 RC QP，并把双方从 `RESET` 依次迁移到 `INIT`、`RTR`、`RTS`。

```bash
make clean && make
build/rdma-qp-state --list
build/rdma-qp-state --device rxe0 --port 1 --gid-index 1
make test
```

阅读顺序：`include/qp_lab.h` -> `src/qp_state.c` -> `docs/QP_STATE_MODEL.md` -> `tests/TEST_RECORD_20260701.md`。

当前测试机给 `ens34` 固定配置 `fe80::34/64`，对应 GID index 1。地址仍为 `tentative` 时必须等待 IPv6 DAD 完成，否则进入 RTR 会超时。项目只迁移状态，不提交 WR。
