# day22 目标与范围

## 1. day22 的真实目标

把一个 `ivshmem-doorbell` PCI 设备稳定带到 arm64 guest 里，并沉淀出下面几类证据：

- `lspci -nn`
- `lspci -vv -nn`
- `dmesg | grep -i pci`
- `/sys/bus/pci/devices`
- QEMU 启动参数

## 2. day22 不做什么

这一天明确 **不做**：

- 不写 `pci_driver`
- 不做 `pci_enable_device()`
- 不做 `pci_iomap()`
- 不做 MMIO 协议
- 不做中断
- 不做 DMA

## 3. 为什么要这样卡边界

因为 day22 的唯一目标是解决“设备有没有被枚举出来”。
如果这一步都没有被独立验证，后面所有驱动代码都会缺少一个稳定平台。
