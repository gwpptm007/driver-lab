# lab-simple-ramdisk Overview

## 实验问题

```text
如何注册一个最小 block device？
userspace 对 /dev/labram0 的读写如何进入驱动？
驱动如何用内存数组模拟 sector read/write？
如何证明它能被 mkfs、mount、fio 当作 block device 使用？
```

## 预期目录

实现阶段建议使用：

```text
lab-simple-ramdisk/
├── README.md
├── docs/
├── driver/
│   ├── labram.c
│   └── Makefile
├── scripts/
│   ├── 00_check_env.sh
│   ├── 01_build.sh
│   ├── 02_run_smoke.sh
│   └── 03_clean.sh
├── records/
└── reports/
```

## 验收命令草案

```bash
sudo insmod driver/labram.ko
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

## 当前边界

- 不做真实磁盘。
- 不做真实 DMA。
- 不做 blk-mq。
- 不做性能优化。
- 只做最小 block device 功能闭环。

