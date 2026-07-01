# QP State Machine 完整测试记录

## 环境与目录

```text
Host: wq7@192.168.65.135
Path: /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-qp-state-machine
Device: rxe0/1 -> ens34
GID index: 1
```

测试前固定测试网卡地址并等待 DAD：

```bash
sudo ip -6 addr add fe80::34/64 dev ens34
while ip -6 addr show ens34 | grep -q tentative; do sleep 1; done
ibv_devinfo -d rxe0 -v | grep -A10 GID
```

## 编译和执行命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-qp-state-machine
make clean
make
build/rdma-qp-state --device rxe0 --port 1 --gid-index 1
make test
```

## 实际状态机输出

```text
endpoint=left qp_num=244 state=RESET
endpoint=right qp_num=245 state=RESET
device=rxe0 port=1 gid_index=1
negative=RESET_to_RTR expected=failure actual=failure errno=22 result=pass
endpoint=left qp_num=244 state=INIT
endpoint=right qp_num=245 state=INIT
endpoint=left qp_num=244 state=RTR
endpoint=right qp_num=245 state=RTR
endpoint=left qp_num=244 state=RTS
endpoint=right qp_num=245 state=RTS
state_machine_result=pass
cleanup=complete result=pass
```

QPN 每次运行会变化。

## GID 边界记录

使用 `--gid-index 0` 时实际失败：

```text
transition=left_INIT_to_RTR rc=110 errno=110 message=Connection timed out
cleanup=complete result=fail
```

检查命令：

```bash
ip addr show ens34
ibv_devinfo -d rxe0 -v | grep -A20 GID
```

确认 `ens34` 固定 link-local 地址 `fe80::34` 对应 GID index 1。地址完成 DAD、`tentative` 标志消失后，使用 index 1 通过。

## 自动测试验收

```text
PASS: help, invalid transition, two endpoints RTS, cleanup
```

结论：非法跳转被拒绝，两个 RC QP 均完整经历 RESET、INIT、RTR、RTS，随后逆序清理。
