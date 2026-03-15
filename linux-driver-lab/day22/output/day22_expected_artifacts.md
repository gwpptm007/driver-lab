# day22 预期产物

一次完整运行后，建议至少出现这些文件：

```text
records/<run-id>/
├── dmesg-pci.txt
├── kernel-config-check.txt
├── lspci-nn.txt
├── lspci-vv-nn.txt
├── pci-config-dump.txt
├── qemu-command.txt
├── qemu.stderr.log
├── qemu.stdout.log
├── run-summary.md
├── serial.log
├── server.log
└── sysfs-pci-devices.txt
```

## 它们各自说明什么

- `lspci-nn.txt`
  - 设备是否真的被 guest 看到
- `lspci-vv-nn.txt`
  - 设备详细能力信息
- `dmesg-pci.txt`
  - PCI 枚举过程是否健康
- `sysfs-pci-devices.txt`
  - 内核 sysfs 视角是否已挂出设备
- `pci-config-dump.txt`
  - config 空间是否可读
- `qemu-command.txt`
  - 本次运行的设备参数是否正确
- `serial.log`
  - 第一现场原始日志
