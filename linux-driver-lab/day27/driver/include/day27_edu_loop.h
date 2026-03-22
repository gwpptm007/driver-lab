/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY27_EDU_LOOP_H
#define DAY27_EDU_LOOP_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#define DAY27_DRV_NAME          "day27_edu_loop"
#define DAY27_CLASS_NAME        "day27_edu"
#define DAY27_DEV_NAME_FMT      "day27_edu%d"

#define DAY27_EDU_VENDOR_ID     0x1234
#define DAY27_EDU_DEVICE_ID     0x11e8

/* QEMU EDU register map: enough for Day27 loop smoke. */
#define DAY27_EDU_REG_IDENTITY      0x00
#define DAY27_EDU_REG_LIVENESS      0x04
#define DAY27_EDU_REG_IRQ_STATUS    0x24
#define DAY27_EDU_REG_IRQ_ACK       0x64
#define DAY27_EDU_REG_IRQ_RAISE     0x60

struct day27_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    unsigned int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;

    dev_t devt;
    struct cdev cdev;
    struct device *device;
};

#endif
