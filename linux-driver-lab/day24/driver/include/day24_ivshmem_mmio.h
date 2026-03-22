/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY24_IVSHMEM_MMIO_H
#define DAY24_IVSHMEM_MMIO_H

#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/types.h>

#include "../../include/day24_ivshmem_uapi.h"

#define DAY24_DRV_NAME      "day24_ivshmem_mmio"
#define DAY24_DEVICE_NAME   "day24_ivshmem0"
#define DAY24_PROTO_HDR_LEN DAY24_PROTO_PAYLOAD_OFF

struct day24_bar_info {
    int index;
    resource_size_t start;
    resource_size_t end;
    resource_size_t len;
    unsigned long flags;
    void __iomem *vaddr;
};

struct day24_dev {
    struct pci_dev *pdev;
    struct day24_bar_info bar[PCI_STD_NUM_BARS];
    struct miscdevice miscdev;
    struct mutex lock;
    u32 bar0_first_dword;
    bool regions_claimed;
    bool device_enabled;
};

#endif /* DAY24_IVSHMEM_MMIO_H */
