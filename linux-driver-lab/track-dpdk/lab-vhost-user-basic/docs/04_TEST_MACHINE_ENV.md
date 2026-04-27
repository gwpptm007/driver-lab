# 04_TEST_MACHINE_ENV

本实验沿用 `track-dpdk/docs/00_ENVIRONMENT_PREPARE.md` 中的测试机环境。

## 测试机

```text
Guest OS: Ubuntu 22.04.5 Desktop
Kernel:   Linux 6.8.0-110-generic
User:     wq7
SSH IP:   192.168.65.135
```

## 网卡规划

| 接口 | 驱动 | PCI | 用途 | 本实验是否使用 |
|---|---|---|---|---|
| ens33 | e1000 | 0000:02:01.0 | SSH/NAT 管理口 | 不操作 |
| ens34 | e1000 | 0000:02:02.0 | 备用 | 不操作 |
| ens192 | vmxnet3 | 0000:0b:00.0 | DPDK 物理 PMD 实验 | 不操作 |

## 本实验只使用

```text
hugepage
dpdk-testpmd
net_vhost vdev
/tmp/dpdk-vhost-user0 UNIX socket
```

因此它可以在 `ens192` 已经 bind 到 `uio_pci_generic` 或恢复为 `vmxnet3` 的情况下运行。
