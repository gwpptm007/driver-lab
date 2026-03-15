# records 说明

这里放 **真实运行证据**，不放计划文档。

建议目录结构：

```text
records/
└── 20260315-223000/
    ├── lspci-nn.txt
    ├── lspci-vv-nn.txt
    ├── dmesg-pci.txt
    ├── sysfs-pci-devices.txt
    ├── qemu-command.txt
    ├── serial.log
    ├── server.log
    └── run-summary.md
```

## 原则

- 一次运行一个目录
- 原始日志优先保留
- 结论型摘要不能替代原始证据
