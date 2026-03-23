/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY30_EDU_MMAP_H
#define DAY30_EDU_MMAP_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#define DAY30_DRV_NAME                 "day30_edu_mmap"
#define DAY30_CLASS_NAME               "day30_edu"
#define DAY30_DEV_NAME_FMT             "day30_edu%d"

#define DAY30_EDU_VENDOR_ID            0x1234
#define DAY30_EDU_DEVICE_ID            0x11e8

/* QEMU EDU MMIO register map used by Day30. */
#define DAY30_EDU_REG_IDENTITY         0x00
#define DAY30_EDU_REG_LIVENESS         0x04
#define DAY30_EDU_REG_IRQ_STATUS       0x24
#define DAY30_EDU_REG_IRQ_ACK          0x64
#define DAY30_EDU_REG_DMA_SRC          0x80
#define DAY30_EDU_REG_DMA_DST          0x88
#define DAY30_EDU_REG_DMA_COUNT        0x90
#define DAY30_EDU_REG_DMA_CMD          0x98

#define DAY30_EDU_DEVBUF_OFFSET        0x40000ULL

#define DAY30_EDU_DMA_CMD_START        0x01
#define DAY30_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY30_EDU_DMA_CMD_IRQ          0x04

/*
 * Day30 延续 day29 的 DMA 布局，但把“谁写/谁读”换成用户态主导：
 * - src_off: 用户态通过 mmap 写入 pattern
 * - dst_off: 用户态通过 mmap 读取回写结果
 */
#define DAY30_DMA_MASK_BITS_DEFAULT    32
#define DAY30_DMA_BYTES                4096
#define DAY30_DMA_SRC_OFF              0
#define DAY30_DMA_DST_OFF              2048
#define DAY30_DMA_VERIFY_MAX           2048

struct day30_dev {
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

    u32 last_run_len;
    u32 last_run_seed;
    s32 last_run_error;
    u32 last_run_ok;
    u32 last_irq_delta;
    u32 last_dma_cmd;

    /*
     * mmap 结果单独记录。
     * 这样 records 里既能知道 DMA 是否跑通，也能知道 mmap 边界校验是否按预期工作。
     */
    u32 last_mmap_ok;
    s32 last_mmap_error;
    u32 last_mmap_len;
    u32 last_mmap_pgoff;

    struct mutex op_lock;

    dev_t devt;
    struct cdev cdev;
    struct device *device;
};

#endif
