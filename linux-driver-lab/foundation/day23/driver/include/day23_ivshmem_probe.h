/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY23_IVSHMEM_PROBE_H
#define DAY23_IVSHMEM_PROBE_H

#include <linux/pci.h>
#include <linux/types.h>

#define DAY23_IVSHMEM_VENDOR_ID 0x1af4
#define DAY23_IVSHMEM_DEVICE_ID 0x1110
#define DAY23_DRV_NAME          "day23_ivshmem_probe"

struct day23_bar_info {
    int index;
    resource_size_t start;
    resource_size_t end;
    resource_size_t len;
    unsigned long flags;
    void __iomem *vaddr;
};

struct day23_dev {
    struct pci_dev *pdev;
    struct day23_bar_info bar[PCI_STD_NUM_BARS];
    u32 bar0_first_dword;
    bool regions_claimed;
    bool device_enabled;
};

#endif /* DAY23_IVSHMEM_PROBE_H */
