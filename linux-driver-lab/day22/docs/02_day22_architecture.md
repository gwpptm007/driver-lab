# day22 架构与数据流

## 1. 总体结构

```text
宿主机
 ├─ day22/scripts/00_check_host_tools.sh
 ├─ day22/scripts/01_check_kernel_config.sh
 ├─ day22/scripts/03_prepare_rootfs.sh
 │   └─ 生成 day22 专属 initramfs
 ├─ day22/scripts/04_start_ivshmem_server.sh
 │   └─ 启动 ivshmem-server，创建 socket
 ├─ day22/scripts/05_run_qemu_ivshmem.sh
 │   └─ 启动 qemu-system-aarch64 -device ivshmem-doorbell
 └─ day22/scripts/06_extract_records.sh
     └─ 从 serial.log 中切分 records

QEMU guest (arm64)
 └─ /init（由 day22 生成）
     ├─ lspci -nn
     ├─ lspci -vv -nn
     ├─ dmesg | grep -i pci
     ├─ ls /sys/bus/pci/devices
     └─ poweroff -f
```

## 2. 为什么 guest 侧要自动运行

因为 day22 想要的是“可重复”而不是“手工一次成功”。
自动化 guest init 的好处：

- 不依赖你手敲命令
- 串口日志结构稳定
- 主机侧容易自动抽取证据
- 以后 day23/day24 也可以沿用这套回归风格

## 3. 为什么 records 从 serial.log 切分

这不是最花哨的方法，但对学习工程非常合适：

- 你能看到 guest 实际输出的原始文本
- 不需要 guest 再额外挂共享盘才能带出结果
- 一旦失败，第一现场就在 serial.log

## 4. day22 与后续几天的关系

- day22：guest 能看到设备
- day23：驱动能接管设备
- day24：驱动能读写 BAR
- day25：驱动能处理中断

所以 day22 是 W4 的“设备 bring-up 日”。
