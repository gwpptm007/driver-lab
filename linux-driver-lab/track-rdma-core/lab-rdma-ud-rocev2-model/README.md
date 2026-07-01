# RDMA UD And RoCEv2 Model

本项目创建两个 UD QP，通过 Address Handle、目标 QPN 和 Q_Key 发送 datagram，并验证接收 buffer 的 40 字节 GRH 前缀。

```bash
make clean && make
build/rdma-ud-demo --device rxe0 --port 1 --gid-index 1
make test
```

阅读顺序：`src/main.c` -> `docs/UD_TRANSPORT_MODEL.md` -> `docs/ROCEV2_PACKET_MODEL.md` -> `tests/TEST_RECORD_20260701.md`。

UD 不建立 RC 式端到端连接，不保证可靠、按序或自动重传；需要应用自行处理丢包和关联。
