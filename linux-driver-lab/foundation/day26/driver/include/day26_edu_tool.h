#ifndef DAY26_EDU_TOOL_H
#define DAY26_EDU_TOOL_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#define DAY26_DRV_NAME "day26_edu_tool"
#define DAY26_CLASS_NAME "day26_edu"
#define DAY26_DEV_NAME_FMT "day26_edu%d"

#define DAY26_EDU_VENDOR_ID 0x1234
#define DAY26_EDU_DEVICE_ID 0x11e8
#define DAY26_BAR0 0

#define DAY26_EDU_REG_ID          0x00
#define DAY26_EDU_REG_LIVENESS    0x04
#define DAY26_EDU_REG_STATUS      0x20
#define DAY26_EDU_REG_IRQ_STATUS  0x24
#define DAY26_EDU_REG_IRQ_RAISE   0x60
#define DAY26_EDU_REG_IRQ_ACK     0x64

struct day26_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;
    int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    u32 identity_value;
    u32 liveness_value;
    u32 liveness_inverted;
    spinlock_t irq_lock;
    dev_t devt;
    struct cdev cdev;
    struct device *device;
};

#endif
