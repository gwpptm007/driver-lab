# day22 详细测试过程

## 1. 设置环境变量

先进入目录：

```bash
cd linux-driver-lab/day22
```

然后至少导出下面三个变量：

```bash
export KERNEL_IMAGE=/path/to/arch/arm64/boot/Image
export BUSYBOX_BIN=/path/to/arm64-busybox
export GUEST_LSPCI_BIN=/path/to/arm64-static-lspci
```

如果你还想做内核配置检查：

```bash
export KERNEL_CONFIG_PATH=/path/to/linux/.config
```

如果你还想把 day22 的 stub 模块也编出来：

```bash
export KDIR=/path/to/kernel/build
```

---

## 2. 先看两份最重要的 C 代码

### 2.1 看 guest 侧枚举工具

```bash
sed -n '1,260p' tools/pci_sysfs_dump.c
```

重点看：

- 为什么它直接读 `/sys/bus/pci/devices`
- 为什么它会打印 `resource`
- 为什么它还会预览 `config` 空间

### 2.2 看 day23 的起步骨架

```bash
sed -n '1,260p' driver/day22_ivshmem_stub.c
```

重点看：

- `pci_device_id`
- `probe/remove`
- `struct day22_stub_dev`
- 为什么 day22 只打印 BAR，不真正 `pci_iomap`

---

## 3. 做一次只检查、不启动的预检

```bash
make check
```

你应该重点看：

- `QEMU_BIN` 是否存在
- `IVSHMEM_SERVER_BIN` 是否存在
- `KERNEL_IMAGE` 是否存在
- `BUSYBOX_BIN` 是否存在
- `GUEST_LSPCI_BIN` 是否被识别成 arm64 静态 ELF
- `GUEST_PCI_SYSFS_DUMP_BIN` 的输出路径是否符合预期

---

## 4. 先单独编译 day22 自己的 guest C 工具

```bash
make build-tools
```

执行后，预期看到：

```text
workdir/tools/aarch64/pci_sysfs_dump
```

你可以手动检查：

```bash
file workdir/tools/aarch64/pci_sysfs_dump
```

理想输出应体现：

- `ARM aarch64`
- `statically linked`

如果这里失败，先不要往下跑 QEMU，先把交叉编译器问题解决。

---

## 5. 构建 day22 专属 initramfs

```bash
make rootfs
```

执行后，预期生成：

```text
workdir/rootfs/
workdir/rootfs.img
```

你可以手动检查：

```bash
find workdir/rootfs -maxdepth 2 -type f | sort
```

至少应能看到：

- `workdir/rootfs/init`
- `workdir/rootfs/bin/busybox`
- `workdir/rootfs/bin/lspci`
- `workdir/rootfs/bin/pci_sysfs_dump`

这一步很关键，因为它能证明：

- day22 的 guest 工具不是放在仓库里看看而已
- 而是真的被打进了 guest rootfs

---

## 6. 一键执行 day22

```bash
make run
```

或者：

```bash
./scripts/07_run_all.sh
```

执行成功后，重点目录是：

```text
workdir/runs/<run-id>/
records/<run-id>/
```

---

## 7. 检查 records

最重要的文件：

```text
records/<run-id>/lspci-nn.txt
records/<run-id>/lspci-vv-nn.txt
records/<run-id>/dmesg-pci.txt
records/<run-id>/sysfs-pci-devices.txt
records/<run-id>/pci-config-dump.txt
records/<run-id>/qemu-command.txt
records/<run-id>/run-summary.md
```

### 7.1 看 `lspci-nn.txt`

应该能看到 `1af4:1110`。

### 7.2 看 `lspci-vv-nn.txt`

应该能看到更详细的 capability / BAR 相关信息。

### 7.3 看 `sysfs-pci-devices.txt`

这份文件里现在会同时包含：

- `ls -l /sys/bus/pci/devices` 的结果
- `pci_sysfs_dump` 输出的 BDF / vendor / device / class / irq / resource / config 预览

### 7.4 看 `dmesg-pci.txt`

应该能看到 PCI bus / host bridge / device 枚举相关日志。

---

## 8. 可选：把 day22 的 stub 模块也编出来

如果你本机已经有内核构建目录：

```bash
make module
```

或者：

```bash
KDIR=/path/to/kernel/build ./scripts/09_build_stub_module.sh
```

预期产物：

```text
driver/day22_ivshmem_stub.ko
```

这里的目的不是今天就把模块插进 guest，而是：

- 验证 day22 目录内已经有真正的 pci_driver C 骨架
- 让 day23 不用从空目录起步

---

## 9. day22 如何人工判断通过

满足下面几点就算通过：

1. `lspci -nn` 里出现 `1af4:1110`
2. `lspci -vv -nn` 已归档
3. `pci_sysfs_dump` 输出里能看到目标设备的 `resource`
4. `pci-config-dump.txt` 里能看到 config 空间预览
5. `driver/day22_ivshmem_stub.c` 已经准备好，可直接给 day23 续写

---

## 10. 如果失败，先看哪几个文件

按优先级建议这样看：

1. `records/<run-id>/serial.log`
2. `records/<run-id>/qemu.stderr.log`
3. `records/<run-id>/server.log`
4. `workdir/rootfs/bin/pci_sysfs_dump` 是否真实存在
5. `driver/day22_ivshmem_stub.c` 是否已经编过 `make module`
