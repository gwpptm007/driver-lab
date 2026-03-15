# day22 实施计划（实做版）

## 1. 今日一句话目标

在 arm64 QEMU `virt` guest 中枚举出 `ivshmem-doorbell` PCI 设备，并把 `lspci -vv -nn` 等证据自动归档到 `records/`。

## 2. 今日最小闭环

### 输入

- 你本机已有的 arm64 `Image`
- 你本机已有的 arm64 BusyBox
- 你本机已有的 arm64 `lspci`（或 `pciutils` 源码）
- QEMU / ivshmem-server

### 过程

1. 主机环境检查
2. 内核 PCI 配置检查
3. 构建 day22 独立 initramfs
4. 启动 ivshmem-server
5. 启动 QEMU
6. guest 自动执行 `lspci` / `dmesg` / `sysfs` 检查
7. 主机切分 serial.log 生成 `records/`

### 输出

- `records/<run-id>/lspci-nn.txt`
- `records/<run-id>/lspci-vv-nn.txt`
- `records/<run-id>/dmesg-pci.txt`
- `records/<run-id>/sysfs-pci-devices.txt`
- `records/<run-id>/run-summary.md`

## 3. 今日的执行顺序

```text
make check
make rootfs
make run
```

## 4. 今日最重要的边界

- 不写 `pci_driver`
- 不写 MMIO 协议
- 不写中断处理
- 只解决“设备可见 + 证据留痕”
