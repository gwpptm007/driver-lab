# 06_TEST_MACHINE_ENV

当前默认对齐前面 DPDK track 的测试机：

```text
Guest: Ubuntu 22.04.5 Desktop
Kernel: 6.8.0-110-generic
Hypervisor: VMware
管理口: ens33 / e1000 / 192.168.65.135
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0
DPDK driver: uio_pci_generic
HugePages: 1024 x 2MB
```

注意：脚本不应该操作 `ens33`，避免断开 SSH/管理链路。
