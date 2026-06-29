# Block I/O Track Implementation Plan

## 总体策略

先做小闭环，再逐步贴近真实内核路径：

```text
simple ramdisk
  -> bio/request path explanation
  -> blk-mq ramdisk
  -> virtio-blk source dive
  -> fio/trace observability
  -> track summary
```

## Phase 1: lab-simple-ramdisk

创建一个教学型 ramdisk block driver，目标是能被 userspace 当成普通 block device 使用。

最小功能：

- register block device。
- allocate memory backend。
- expose `/dev/labram0` 或等价设备。
- handle read/write sectors。
- cleanup on module unload。

验证命令：

```bash
sudo insmod labram.ko
lsblk
sudo dd if=/dev/zero of=/dev/labram0 bs=4K count=16
sudo mkfs.ext4 -F /dev/labram0
sudo mkdir -p /mnt/labram
sudo mount /dev/labram0 /mnt/labram
echo hello | sudo tee /mnt/labram/hello.txt
cat /mnt/labram/hello.txt
sudo fio --name=smoke --filename=/mnt/labram/fio.dat --size=4M --rw=randread --bs=4k --iodepth=1 --runtime=5 --time_based
sudo umount /mnt/labram
sudo rmmod labram
```

## Phase 2: lab-bio-request-path

在 Phase 1 基础上补文档和 trace，把每次 read/write 如何进入驱动说清楚。

验证：

- dmesg 中能看到 read/write sector。
- records 中能对应 userspace 命令和 driver log。

## Phase 3: lab-blk-mq-ramdisk

用 blk-mq 重写 request handling。

验证：

- blk-mq driver 能完成 Phase 1 全部 smoke。
- fio 输出可与 simple ramdisk 对照。

## Phase 4: lab-virtio-blk-source-dive

阅读真实 `virtio_blk`，把自写模型映射到真实驱动。

验证：

- 输出 source mapping matrix。
- 记录 probe、queue setup、request submit、completion 对应函数。

## Phase 5: lab-fio-latency-observe

建立 fio + trace 观测模板。

验证：

- 至少一份 fio latency report。
- 至少一份 iostat/blktrace/bpftrace 记录。

## Phase 6: project-block-io-track-summary

产出最终作品材料。

验证：

- final report。
- evidence index。
- resume material。
- interview notes。
- backlog。

