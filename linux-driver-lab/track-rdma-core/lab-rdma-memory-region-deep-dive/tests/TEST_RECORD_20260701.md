# MR Deep Dive 测试记录

## 测试位置

```text
Host: wq7@192.168.65.135
Path: /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-memory-region-deep-dive
Device: rxe0/1 -> ens34, ACTIVE, Ethernet
```

## 编译命令与输出

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-memory-region-deep-dive
make clean
make
```

```text
rm -rf build
mkdir -p build
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/main.o src/main.c
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/rdma_device.o src/rdma_device.c
cc -Iinclude -I/usr/include/libnl3 -O2 -g -std=c11 -Wall -Wextra -Wpedantic -c -o build/mr_experiments.o src/mr_experiments.c
cc -o build/rdma-mr-lab build/main.o build/rdma_device.o build/mr_experiments.o -libverbs
```

## 手工执行

```bash
build/rdma-mr-lab --list
build/rdma-mr-lab --device rxe0 --port 1
```

完整实际输出：

```text
device=rxe0 port=1 pd=ready
case=local_write access=0x1 offset=0 aligned_4k=yes expected=success actual=success address=0x61ed006f7000 length=8192 lkey=0xd7d9 rkey=0xd7d9 result=pass
case=remote_read_only access=0x4 offset=0 aligned_4k=yes expected=success actual=success address=0x61ed006f7000 length=8192 lkey=0xd87d rkey=0xd87d result=pass
case=remote_write_without_local_write access=0x2 offset=0 aligned_4k=yes expected=failure actual=failure errno=22 message=Invalid argument result=pass
case=all_permissions access=0x7 offset=0 aligned_4k=yes expected=success actual=success address=0x61ed006f7000 length=8192 lkey=0xd90a rkey=0xd90a result=pass
case=unaligned_all_permissions access=0x7 offset=1 aligned_4k=no expected=success actual=success address=0x61ed006f7001 length=8191 lkey=0xda88 rkey=0xda88 result=pass
suite_cases=5 suite_failures=0 suite_result=pass
cleanup=complete result=pass
```

地址、lkey、rkey 每次运行会变化，不作为固定断言。

## 自动测试

```bash
make test
```

```text
bash tests/mr_test.sh
PASS: help
PASS: list
PASS: MR experiment suite
SUMMARY: pass=3 fail=0
```

## 验收结论

- 5 个 MR case 全部符合预期。
- `REMOTE_WRITE` 不带 `LOCAL_WRITE` 被 provider 以 `EINVAL` 拒绝。
- 非页对齐地址在当前 RXE/provider 上注册成功。
- 每个成功 MR 均完成 `ibv_dereg_mr()`，最后销毁 PD/context。
