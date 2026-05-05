# 07_REVIEW_AFTER_TEST

## 判定

`lab-vmxnet3-testpmd` 已完成 DPDK 第一站 smoke test，可以进入 `lab-vhost-user-basic`。

## 关键证据

- `HugePages_Total=1024`，testpmd 运行后 `HugePages_Free` 下降，说明 hugepage 实际被使用。
- `0000:0b:00.0` 已绑定到 `uio_pci_generic`。
- `dpdk-testpmd` 日志出现 `Probe PCI driver: net_vmxnet3`。
- `Port 0` 完成初始化并输出 `NIC statistics for port 0`。
- 管理口 `ens33/e1000` 未被误绑定。

## 已根据测试结果修正

- 当前 VMware Workstation guest 未透传 IOMMU，默认 DPDK driver 从 `vfio-pci` 调整为 `uio_pci_generic`。
- `02_bind_vmxnet3.sh` 在每次 bind 前清空 `BIND_AFTER.txt`，避免旧错误污染新记录。

## 遗留说明

RX/TX 统计为 0 不影响本阶段通过；当前阶段验证的是 PMD 初始化和 records 留证，不是发包压测。
