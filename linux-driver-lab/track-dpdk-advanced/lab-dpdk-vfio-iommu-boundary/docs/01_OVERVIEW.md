# lab-dpdk-vfio-iommu-boundary Overview

## 实验问题

```text
当前测试机是否具备 VFIO/IOMMU 前置条件？
UIO 和 VFIO 的边界是什么？
vmxnet3 在 VMware 环境下为什么要单独记录 RX/TX 边界？
如何避免把环境失败写成代码失败？
```

## 验收

```text
PASS_UIO_VFIO_MATRIX
PASS_VMXNET3_BOUNDARY
PASS_IOMMU_CHECKLIST
```

## 边界

本 Lab 只收集和解释环境，不自动切换 driver，不修改 GRUB，不 bind 管理口。
