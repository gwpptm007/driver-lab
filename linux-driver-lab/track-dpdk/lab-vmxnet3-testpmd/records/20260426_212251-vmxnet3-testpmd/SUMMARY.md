# SUMMARY

## Lab

lab-vmxnet3-testpmd

## 测试机环境

- Guest：Ubuntu 22.04.5 Desktop
- Kernel：Linux 6.8.0-110-generic
- 管理网卡：ens33 / e1000 / 192.168.65.135
- DPDK 网卡：ens192 / vmxnet3 / 0000:0b:00.0 / 192.168.100.1/24
- DPDK driver：uio_pci_generic（本环境 vfio-pci 不可用）

## 目标

- [x] 只读环境检查
- [x] hugepage 配置
- [x] dpdk-devbind 状态确认
- [x] ens192 绑定到 DPDK driver（uio_pci_generic）
- [x] testpmd 启动
- [x] stats/logs 收集

## 执行命令

```bash
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
./scripts/02_bind_vmxnet3.sh status
sudo modprobe uio_pci_generic
sudo DPDK_DRIVER=uio_pci_generic DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
sudo ./scripts/03_run_testpmd.sh
./scripts/04_collect_stats.sh
./scripts/05_make_review_bundle.sh
```

## 关键结果

- hugepage：1024 × 2MB = 2GB（HugePages_Total: 1024, HugePages_Free: 1019）
- bind 前 driver：vmxnet3
- bind 后 driver：uio_pci_generic
- testpmd 是否启动：✅ 成功启动，EAL/PMD/Port 均正常
- stats 是否输出：✅ TESTPMD.log 输出正常（RX/TX=0 因无对端流量）

## 问题

- **vfio-pci 不可用**：VMware Workstation 不支持 IOMMU 透传
- **大页重启丢失**：reboot 后 HugePages_Total 恢复为 0
- **ens192 消失**：绑定后 `ip link show ens192` 显示 "Device does not exist"（正常现象）

## 原因分析

1. **vfio-pci 失败**：VMware Workstation 是 Type-2 Hypervisor，`vhv.enable=TRUE` 仅用于 CPU 虚拟化，不透传物理 IOMMU。`vfio-pci` 依赖 IOMMU DMA 重映射，无法在 VMware 虚拟机中使用。
2. **大页重启丢失**：大页配置通过 `echo 1024 > /proc/sys/vm/nr_hugepages` 实现，非持久化。
3. **ens192 消失**：设备已从 kernel 驱动解绑，由 DPDK 用户空间驱动接管，属于正常现象。

## 修复方法

1. 使用 `uio_pci_generic` 替代 `vfio-pci`（本 Lab 已采用）
2. 将大页配置写入 `/etc/rc.local` 或 systemd service
3. 使用 `dpdk-devbind.py --status` 查看 DPDK 设备状态

## 下一步

- Phase 2: lab-vhost-user-basic
- 理解 DPDK vhost-user socket 架构
- 实现 QEMU 与 DPDK 的 vhost-user 通信
