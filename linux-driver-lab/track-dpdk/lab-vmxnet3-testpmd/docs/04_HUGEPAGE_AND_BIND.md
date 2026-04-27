# 04_HUGEPAGE_AND_BIND

## hugepage 是什么

DPDK 的 mbuf、mempool 和 DMA 相关内存需要大页支持。大页（2MB）相比普通页（4KB）可以：

- **减少 TLB miss**：一个 2MB 页相当于 512 个 4KB 页，大幅减少地址转换开销
- **降低页表开销**：mempool 动辄数百 MB，连续大页效率更高
- **保证 DMA 物理连续性**：网卡 DMA 需要物理地址连续，大页更容易满足

没有 hugepage，`testpmd` 常见表现是：

```
EAL: FATAL: Cannot get hugepage information.
EAL: Cannot init EAL: Permission denied
```

> ⚠️ 重启后大页配置会丢失，需要重新配置。建议将配置写入 `/etc/rc.local` 或 systemd service 实现开机自动配置。

## 当前 Lab 默认 hugepage

```bash
sudo HUGEPAGES=1024 ./scripts/01_setup_hugepages.sh
```

脚本默认做三件事：

```text
1. mkdir -p /mnt/huge
2. mount -t hugetlbfs nodev /mnt/huge
3. echo 1024 > /proc/sys/vm/nr_hugepages
```

验证：

```bash
grep Huge /proc/meminfo
mount | grep hugetlbfs
```

**预期输出**：

```
HugePages_Total:    1024
HugePages_Free:     1024
Hugepagesize:       2048 kB
Hugetlb:         2097152 kB

hugetlbfs on /dev/hugepages type hugetlbfs (rw,relatime,pagesize=2M)
nodev on /mnt/huge type hugetlbfs (rw,relatime,pagesize=2M)
```

## bind 是什么

Linux 默认由内核驱动 `vmxnet3` 管理 `ens192`：

```
VMXNET3 PCI device → kernel vmxnet3 driver → net_device ens192 → Linux 协议栈
```

DPDK 接管后变成：

```
VMXNET3 PCI device → uio_pci_generic → DPDK PMD → testpmd/app
                                    ↓
                          ens192 从 Linux 网络栈消失
```

> 注意：绑定到 DPDK 后，`ens192` 不再是普通 Linux 网络接口，因为设备已经从内核网卡驱动解绑。`ip link show ens192` 会显示 "Device does not exist"。

## 查看绑定状态

```bash
./scripts/02_bind_vmxnet3.sh status
dpdk-devbind.py --status
```

**绑定前**：

```
0000:0b:00.0 'VMXNET3 Ethernet Controller' if=ens192 drv=vmxnet3 unused= *Active*
```

**绑定后**：

```
0000:0b:00.0 'VMXNET3 Ethernet Controller' drv=uio_pci_generic unused=vmxnet3
```

## 绑定到 uio_pci_generic

> ⚠️ VMware Workstation 不支持 IOMMU，`vfio-pci` 无法使用。使用 `uio_pci_generic` 作为替代方案。

```bash
# 1. 加载 uio_pci_generic 驱动
sudo modprobe uio_pci_generic

# 2. 绑定到 uio_pci_generic
sudo DPDK_DRIVER=uio_pci_generic DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
```

**完整验证**：

```bash
# 方法1：dpdk-devbind.py 查看
dpdk-devbind.py --status

# 方法2：ip link 确认设备消失
ip link show ens192
# 预期：Device "ens192" does not exist.

# 方法3：lspci 查看驱动
lspci -k -s 0000:0b:00.0
# 预期：Kernel driver in use: uio_pci_generic

# 方法4：查看 sysfs
ls /sys/bus/pci/drivers/uio_pci_generic/0000:0b:00.0/
```

## 从 DPDK 恢复到内核 vmxnet3

```bash
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh unbind
```

恢复后验证：

```bash
sudo ip link set ens192 up
ip -br addr show ens192
ethtool -i ens192
```

如果 IP 丢失，可按 `track-dpdk/docs/00_ENVIRONMENT_PREPARE.md` 中 netplan 配置恢复。

## IOMMU / vfio-pci 注意事项

`vfio-pci` 需要硬件 IOMMU（Intel VT-d 或 AMD-Vi）支持。

**VMware 环境限制**：

- VMware Workstation 是 Type-2 Hypervisor（运行在宿主机操作系统之上）
- 它不会把物理 IOMMU 透传给虚拟机
- 即使 `vhv.enable = "TRUE"` 已启用，也只是 CPU 虚拟化加速，不是 IOMMU 透传

**验证 IOMMU 状态**：

```bash
cat /proc/cmdline | grep iommu
dmesg | grep -Ei 'DMAR|IOMMU|vfio'
ls /sys/kernel/iommu_groups/
```

**如果 IOMMU 不可用**：

| 驱动 | IOMMU 依赖 | 适用场景 |
|------|-----------|---------|
| `vfio-pci` | 必须 | 有 IOMMU 的物理机或 vSphere/ESXi |
| `uio_pci_generic` | 不需要 | VMware Workstation、VirtualBox、无 IOMMU 环境 |
| `igb_uio` | 不需要 | 需要编译，DPDK 源码提供 |

本 Lab 在 VMware 环境下使用 `uio_pci_generic`，这是无需 IOMMU 的可行方案。
