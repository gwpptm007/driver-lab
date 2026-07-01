# One-Sided RDMA 完整测试记录

```text
Host: wq7@192.168.65.135
Path: /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-one-sided-read-write
Device: rxe0/1, GID index 1, ens34 address fe80::34
```

## 环境准备

```bash
sudo ip -6 addr add fe80::34/64 dev ens34
while ip -6 addr show ens34 | grep -q tentative; do sleep 1; done
```

## 编译与执行

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-one-sided-read-write
make clean
make
build/rdma-one-sided --device rxe0 --port 1 --gid-index 1
make test
```

## 实际完整输出

```text
connection=RTS left_qpn=269 right_qpn=270
remote_metadata address=0x57e0f5cbd000 rkey=0xf40f length=256
operation=RDMA_WRITE cqe_wr_id=301 status=success opcode=1
operation=RDMA_WRITE remote_payload=written-by-left-with-rdma-write verify=pass
operation=RDMA_READ cqe_wr_id=401 status=success opcode=2
operation=RDMA_READ local_payload=read-from-right-with-rdma-read verify=pass
one_sided_result=pass
cleanup=complete result=pass
```

QPN、地址和 rkey 每次运行会变化。WRITE/READ 各只有一个发起端 CQE，远端没有 post receive 和 RECV CQE。

## 自动测试

```text
PASS: one-sided RDMA WRITE and READ
```

结论：WRITE 修改远端 buffer，READ 将远端内容拉回本地，两次本地 CQE 和 payload 校验均成功。
