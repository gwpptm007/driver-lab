# RDMA Verbs Object Lifecycle 完整测试记录

## 1. 测试结论

| 项目 | 结果 |
| --- | --- |
| 干净编译 | PASS，无 warning/error |
| RDMA device 枚举 | PASS，发现 `rxe0` |
| context/PD/MR/CQ/QP 生命周期 | PASS |
| RC QP 初始状态 | PASS，`RESET` |
| 参数错误和不存在设备 | PASS，退出码为 1 |
| 自动测试 | PASS，`6/6` |
| 100 次重复创建/销毁 | PASS，资源行数 `1 -> 1` |

记录时间：`2026-07-01T22:06:34+08:00`。

## 2. 编译和执行位置

测试机：

```text
IP:     192.168.65.135
Host:   wq7-virtual-machine
User:   wq7
OS:     Ubuntu 22.04
Kernel: 6.8.0-124-generic
Arch:   x86_64
```

所有编译、运行和测试均在测试机执行，绝对路径为：

```text
/home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle
```

登录并进入目录：

```bash
ssh wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle
```

环境确认命令：

```bash
date -Is
hostname
pwd
id
uname -a
```

实际输出：

```text
2026-07-01T22:06:34+08:00
wq7-virtual-machine
/home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle
uid=1000(wq7) gid=1000(wq7) groups=1000(wq7),4(adm),24(cdrom),27(sudo),30(dip),46(plugdev),109(kvm),122(lpadmin),135(lxd),136(sambashare)
Linux wq7-virtual-machine 6.8.0-124-generic #124~22.04.1-Ubuntu SMP PREEMPT_DYNAMIC Tue May 26 21:05:19 UTC x86_64 x86_64 x86_64 GNU/Linux
```

## 3. 工具链与 RDMA 环境

执行：

```bash
cc --version | head -n 1
make --version | head -n 1
pkg-config --modversion libibverbs
pkg-config --cflags --libs libibverbs
test -f /usr/include/infiniband/verbs.h && echo VERBS_HEADER_PRESENT
rdma link
ibv_devices
ibv_devinfo -d rxe0
```

实际关键输出：

```text
cc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0
GNU Make 4.3
1.14.39.0
-I/usr/include/libnl3 -libverbs
VERBS_HEADER_PRESENT

link rxe0/1 state ACTIVE physical_state LINK_UP netdev ens34

device             node GUID
------             ----------------
rxe0               020c29fffef8f678

hca_id: rxe0
phys_port_cnt: 1
port: 1
state: PORT_ACTIVE (4)
max_mtu: 4096 (5)
active_mtu: 1024 (3)
link_layer: Ethernet
```

测试机重启后 `rxe0` 可能消失。确认 `ens34` 存在后恢复：

```bash
ip -br link
sudo modprobe rdma_rxe
sudo rdma link add rxe0 type rxe netdev ens34
rdma link
ibv_devices
```

如果 `rxe0` 已存在，不要重复执行 `rdma link add`。

## 4. 被测试源码

执行：

```bash
find . -maxdepth 2 -type f | grep -v '^./build/' | sort
```

实际输出：

```text
./.gitignore
./Makefile
./README.md
./docs/ARCHITECTURE.md
./docs/VERBS_OBJECT_MODEL.md
./include/rdma_resources.h
./src/main.c
./src/rdma_device.c
./src/rdma_memory.c
./src/rdma_queue.c
./tests/lifecycle_test.sh
./tests/TEST_RECORD_20260701.md
```

## 5. 干净编译完整记录

执行：

```bash
make clean
make
```

实际完整输出：

```text
rm -rf build
mkdir -p build
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/main.o src/main.c
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/rdma_device.o src/rdma_device.c
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/rdma_memory.o src/rdma_memory.c
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/rdma_queue.o src/rdma_queue.c
cc -o build/rdma-lifecycle build/main.o build/rdma_device.o build/rdma_memory.o build/rdma_queue.o -libverbs
```

结果：生成 `build/rdma-lifecycle`，编译器报告 `0 warning / 0 error`。

## 6. 手工执行完整记录

### 6.1 查看帮助

```bash
build/rdma-lifecycle --help
```

预期首行：

```text
usage: rdma-lifecycle [--list] [--device NAME] [--port N]
```

### 6.2 枚举设备

执行：

```bash
build/rdma-lifecycle --list
```

实际输出：

```text
device_count=1
device[0]=rxe0
```

### 6.3 创建和销毁全部对象

执行：

```bash
build/rdma-lifecycle --device rxe0 --port 1
```

实际完整输出：

```text
device=rxe0
port=1 state=4 active_mtu=3
context=ready
pd=ready
mr=ready address=0x5ac541eae000 length=4096 lkey=0x6c1c rkey=0x6c1c
cq=ready depth=16
qp=ready qp_num=123 qp_type=RC
qp_state=RESET
cleanup=complete
result=pass
```

`address`、`lkey/rkey` 和 `qp_num` 由本次进程/provider 动态生成，每次运行可能变化，不能作为固定测试值。

| 输出 | 对应动作 | 实现文件 |
| --- | --- | --- |
| `context=ready` | 打开 device/context | `src/rdma_device.c` |
| `port=1` | 查询 port | `src/rdma_device.c` |
| `pd=ready` | 创建 PD | `src/rdma_memory.c` |
| `mr=ready` | 分配 buffer、注册 MR | `src/rdma_memory.c` |
| `cq=ready` | 创建 CQ | `src/rdma_queue.c` |
| `qp=ready` | 创建 RC QP | `src/rdma_queue.c` |
| `qp_state=RESET` | 查询 QP 初始状态 | `src/rdma_queue.c` |
| `cleanup=complete` | 逆序销毁资源 | `rdma_resources_cleanup()` |

## 7. 负向测试记录

### 7.1 非法端口

执行：

```bash
build/rdma-lifecycle --port 0
echo $?
```

实际输出：

```text
error=invalid_port value=0
1
```

### 7.2 不存在的设备

执行：

```bash
build/rdma-lifecycle --device definitely-not-an-rdma-device
echo $?
```

实际输出：

```text
error=device_not_found device=definitely-not-an-rdma-device
cleanup=complete
result=fail
1
```

两个错误场景均返回非零。设备选择失败后仍进入统一 cleanup，验证资源结构能够处理部分初始化状态。

## 8. 自动测试完整记录

执行：

```bash
make test
```

也可以直接运行：

```bash
bash tests/lifecycle_test.sh
```

实际完整输出：

```text
bash tests/lifecycle_test.sh
PASS: help
PASS: unknown option
PASS: invalid port
PASS: list devices
PASS: missing device
PASS: real verbs lifecycle (rxe0)
SUMMARY: pass=6 fail=0 skip=0
```

测试覆盖：帮助、未知参数、非法端口、设备枚举、不存在设备和真实 verbs 生命周期。

如果没有 provider-visible device，真实生命周期项会显示 SKIP；环境缺失不会伪装成 PASS。

## 9. 100 次稳定性测试

执行：

```bash
before=$(rdma resource show | wc -l)

i=1
while [ "$i" -le 100 ]; do
    build/rdma-lifecycle --device rxe0 --port 1 >/dev/null || exit 1
    i=$((i + 1))
done

after=$(rdma resource show | wc -l)

echo "loop_count=100"
echo "resource_lines_before=$before"
echo "resource_lines_after=$after"
test "$before" -eq "$after"
echo "loop_test_rc=$?"
```

实际输出：

```text
loop_count=100
resource_lines_before=1
resource_lines_after=1
loop_test_rc=0
```

100 次均成功，循环前后 `rdma resource show` 行数一致。这是实验级明显残留检查，不等价于生产级内存泄漏证明。

## 10. 一次性复现顺序

```bash
ssh wq7@192.168.65.135
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle

rdma link
ibv_devices

make clean
make

build/rdma-lifecycle --list
build/rdma-lifecycle --device rxe0 --port 1
make test
```

最终验收标志：

```text
qp_state=RESET
cleanup=complete
result=pass
SUMMARY: pass=6 fail=0 skip=0
```

## 11. 当前未覆盖范围

- QP `RESET -> INIT -> RTR -> RTS` 状态迁移。
- 两端 QPN/GID/PSN 交换。
- `ibv_post_recv()`、`ibv_post_send()` 和 CQ polling。
- RDMA READ、WRITE、atomic。
- 跨主机真实 RNIC/RoCE 链路。

这些内容应由后续独立项目验证，不混入当前对象生命周期实验。
