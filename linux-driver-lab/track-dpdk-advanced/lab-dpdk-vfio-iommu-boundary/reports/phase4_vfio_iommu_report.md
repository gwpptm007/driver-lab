# Phase 4 Report: VFIO / IOMMU Boundary

## 目标

把 UIO、VFIO、IOMMU、MSI-X、vmxnet3 的关系讲清楚，并在当前测试机上留下真实环境证据。

本阶段不做危险的真实网卡 bind/unbind，不影响管理网卡。

## 记录

```text
records/20260629-212638-vfio-iommu/
```

## 结论

```text
PASS_UIO_VFIO_MATRIX
PASS_VMXNET3_BOUNDARY
PASS_IOMMU_CHECKLIST
```

当前测试机事实：

- kernel cmdline 没有 `intel_iommu=on` 或 `amd_iommu=on`。
- `/sys/kernel/iommu_groups` 没有设备条目。
- `vfio` / `uio` 模块当前未加载。
- `ens192` 是目标实验网卡，仍由 `vmxnet3` kernel driver 管理。
- `ens33` 是管理网卡，不能随意从 kernel driver 解绑。

## 工程判断

这个阶段的价值是“边界清楚”，不是“强行绑定 VFIO”。

在当前 VMware 测试机上，最诚实的结论是：

```text
VFIO/IOMMU prerequisites are not satisfied in the current boot.
The project records the boundary and preserves a checklist for a real NIC / cloud host / IOMMU-enabled VM.
```

## 后续如果要继续做真实 VFIO

需要新的环境或重启参数：

- BIOS/虚拟化平台开启 IOMMU。
- kernel cmdline 增加 `amd_iommu=on` 或 `intel_iommu=on`。
- 确认 `/sys/kernel/iommu_groups` 非空。
- 确认目标 NIC 不是 SSH 管理网卡。
- 使用 `dpdk-devbind.py` 前先记录回滚路径。

