# RC Ping-Pong 完整测试记录

```text
Host: wq7@192.168.65.135
Path: /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-rc-pingpong
Device: rxe0/1, GID index 1 (fe80::34)
```

## 命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-rc-pingpong
make clean
make
build/rdma-rc-pingpong --device rxe0 --port 1 --gid-index 1
make test
```

## 实际完整输出

```text
connection=RTS left_qpn=255 right_qpn=256
round=ping cqe_wr_id=102 status=success opcode=128 byte_len=15
round=ping cqe_wr_id=101 status=success opcode=0 byte_len=15
round=ping receiver=right payload=ping-from-left verify=pass
round=pong cqe_wr_id=202 status=success opcode=128 byte_len=16
round=pong cqe_wr_id=201 status=success opcode=0 byte_len=16
round=pong receiver=left payload=pong-from-right verify=pass
pingpong_result=pass
cleanup=complete result=pass
```

QPN 与 CQE 出现顺序可能变化；测试断言状态、payload 和最终结果，不固定动态 QPN。

## 自动测试

```text
PASS: RC ping and pong with successful CQEs
```

结论：双方 QP 到达 RTS，ping/pong 的 SEND 和 RECV CQE 均成功，接收 buffer 内容与发送内容一致。
