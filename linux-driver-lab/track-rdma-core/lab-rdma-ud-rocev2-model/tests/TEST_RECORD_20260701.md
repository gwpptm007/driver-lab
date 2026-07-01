# UD/RoCEv2 完整测试记录

```text
Host: wq7@192.168.65.135
Path: /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-ud-rocev2-model
Device: rxe0/1, GID index 1, fe80::34
```

## 环境、编译与执行

```bash
sudo ip -6 addr add fe80::34/64 dev ens34
while ip -6 addr show ens34 | grep -q tentative; do sleep 1; done

cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-ud-rocev2-model
make clean
make
build/rdma-ud-demo --device rxe0 --port 1 --gid-index 1
make test
```

## 实际完整输出

```text
cqe_wr_id=702 status=success opcode=128 byte_len=64
cqe_wr_id=701 status=success opcode=0 byte_len=24
transport=UD qkey=0x11111111 sender_qpn=277 receiver_qpn=278
grh_bytes=40 payload=ud-datagram-over-rocev2 verify=pass
ud_result=pass
cleanup=complete result=pass
```

QPN 每次运行变化。RECV `byte_len=64`，由 40 字节 GRH 和 24 字节 payload 组成。

## 自动测试

```text
PASS: UD datagram, GRH offset and payload
```

结论：UD QP、AH、Q_Key、remote QPN、SEND/RECV CQE 和 GRH payload 偏移均验证成功。
