/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY29_EDU_DMA_H
#define DAY29_EDU_DMA_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#define DAY29_DRV_NAME              "day29_edu_dma"
#define DAY29_CLASS_NAME            "day29_edu"
#define DAY29_DEV_NAME_FMT          "day29_edu%d"

#define DAY29_EDU_VENDOR_ID         0x1234
#define DAY29_EDU_DEVICE_ID         0x11e8

/* QEMU EDU MMIO register map used by Day29. */
#define DAY29_EDU_REG_IDENTITY      0x00
#define DAY29_EDU_REG_LIVENESS      0x04
#define DAY29_EDU_REG_IRQ_STATUS    0x24
#define DAY29_EDU_REG_IRQ_ACK       0x64
#define DAY29_EDU_REG_DMA_SRC       0x80
#define DAY29_EDU_REG_DMA_DST       0x88
#define DAY29_EDU_REG_DMA_COUNT     0x90
#define DAY29_EDU_REG_DMA_CMD       0x98

/* EDU internal device buffer offset. */
#define DAY29_EDU_DEVBUF_OFFSET     0x40000ULL

#define DAY29_EDU_DMA_CMD_START     0x01
#define DAY29_EDU_DMA_CMD_DIR_TO_RAM 0x02
#define DAY29_EDU_DMA_CMD_IRQ       0x04

#define DAY29_DMA_MASK_BITS_DEFAULT 32
#define DAY29_DMA_BYTES             4096
#define DAY29_DMA_SRC_OFF           0
#define DAY29_DMA_DST_OFF           2048
#define DAY29_DMA_VERIFY_MAX        2048

struct day29_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    unsigned int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;

    void *dma_virt;
    dma_addr_t dma_handle;
    size_t dma_bytes;
    u32 dma_mask_bits;

    u32 last_verify_len;
    u32 last_verify_seed;
    s32 last_verify_error;
    u32 last_verify_ok;
    s32 last_mismatch_index;
    u8 last_mismatch_expected;
    u8 last_mismatch_actual;
    u32 last_irq_delta;
    u32 last_dma_cmd;

    struct mutex op_lock;

    dev_t devt;
    struct cdev cdev;
    struct device *device;
};

#endif
