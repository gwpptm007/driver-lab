#ifndef DAY25_EDU_IRQ_H
#define DAY25_EDU_IRQ_H

#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

/* 模块/设备命名约定 */
#define DAY25_DRV_NAME "day25_edu_irq"
#define DAY25_CLASS_NAME "day25_edu"
#define DAY25_DEV_NAME_FMT "day25_edu%d"
#define DAY25_MAX_MINORS 32

/* QEMU EDU teaching device PCI ID */
#define DAY25_EDU_VENDOR_ID 0x1234
#define DAY25_EDU_DEVICE_ID 0x11e8

/* Day25 只用 BAR0。EDU 的控制寄存器都在 BAR0。 */
#define DAY25_BAR0 0

/*
 * EDU MMIO register layout (subset)
 * 这里只列出 day25 需要访问的寄存器：
 * - ID / LIVENESS：probe 阶段做“设备活着 + MMIO 可访问”检查
 * - IRQ_STATUS / IRQ_RAISE / IRQ_ACK：day25 中断实验的核心
 */
#define DAY25_EDU_REG_ID          0x00
#define DAY25_EDU_REG_LIVENESS    0x04
#define DAY25_EDU_REG_STATUS      0x20
#define DAY25_EDU_REG_IRQ_STATUS  0x24
#define DAY25_EDU_REG_IRQ_RAISE   0x60
#define DAY25_EDU_REG_IRQ_ACK     0x64

/*
 * 每个 EDU 设备在驱动中的运行时状态。
 * 这里把“PCI 资源”、“中断状态”、“字符设备状态”放在一起，
 * 方便 probe/remove 完整管理。
 */
struct day25_dev {
    struct pci_dev *pdev;

    /* BAR0 MMIO 映射及其物理资源信息 */
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    /* 中断相关状态 */
    int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    u32 liveness_value;
    u32 liveness_inverted;
    spinlock_t irq_lock;

    /* 字符设备节点相关状态 */
    dev_t devt;
    struct cdev cdev;
    struct device *device;
};

#endif
