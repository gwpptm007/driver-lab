# RDMA One-Sided READ/WRITE

本项目使用两个本地 RC QP 验证 one-sided 数据路径：左端 RDMA WRITE 直接修改右端 MR，再通过 RDMA READ 把右端 MR 拉回左端。

```bash
make clean && make
build/rdma-one-sided --device rxe0 --port 1 --gid-index 1
make test
```

阅读顺序：`src/main.c` -> `docs/ONE_SIDED_MODEL.md` -> `docs/ARCHITECTURE.md` -> `tests/TEST_RECORD_20260701.md`。

本实验没有 post receive，远端也没有 Receive CQE。发起端 WR 必须携带远端虚拟地址和 rkey。
