# VFIO / IOMMU boundary Summary

| Item | Result |
|------|--------|
| PASS_UIO_VFIO_MATRIX | PASS |
| PASS_VMXNET3_BOUNDARY | PASS |
| PASS_IOMMU_CHECKLIST | PASS |

## Current facts

- cmdline=BOOT_IMAGE=/boot/vmlinuz-6.8.0-124-generic root=UUID=b6dfcf48-79dd-42f6-867f-0e7878e613e9 ro quiet splash
- iommu_group_entries=0
- vfio_module_loaded=no
- uio_module_loaded=no
- dpdk_devbind_line=DPDK_PCI=0000:0b:00.0
- vmxnet3_line=	Kernel driver in use: vmxnet3 

## Matrix

| Driver path | Current status | Boundary |
|-------------|----------------|----------|
| uio_pci_generic | usable for prior DPDK basic path | no IOMMU isolation; VMware RX/interrupt caveat remains |
| vfio-pci | checklist only | requires IOMMU enabled and viable group isolation |
| vmxnet3 kernel driver | management/normal kernel path | not DPDK userspace PMD path |

## Checklist

- Confirm kernel cmdline has IOMMU option when using VFIO.
- Confirm /sys/kernel/iommu_groups is non-empty.
- Confirm target PCI function is not management NIC.
- Confirm bind/unbind plan before touching real NIC.
- Record RX/TX behavior separately; TX PASS does not imply RX PASS.
