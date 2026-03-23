/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY34_EDU_STABILITY_H
#define DAY34_EDU_STABILITY_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define DAY34_DRV_NAME                 "day34_edu_stability"
#define DAY34_CLASS_NAME               "day34_edu"
#define DAY34_DEV_NAME_FMT             "day34_edu%d"

#define DAY34_EDU_VENDOR_ID            0x1234
#define DAY34_EDU_DEVICE_ID            0x11e8

#define DAY34_EDU_REG_IRQ_STATUS       0x24
#define DAY34_EDU_REG_IRQ_ACK          0x64
#define DAY34_EDU_REG_DMA_SRC          0x80
#define DAY34_EDU_REG_DMA_DST          0x88
#define DAY34_EDU_REG_DMA_COUNT        0x90
#define DAY34_EDU_REG_DMA_CMD          0x98

#define DAY34_EDU_DEVBUF_OFFSET        0x40000ULL
#define DAY34_EDU_DMA_CMD_START        0x01
#define DAY34_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY34_EDU_DMA_CMD_IRQ          0x04

/*
 * Day34 沿用 day33 的布局：一页 DMA 缓冲区一分为二，src/dst 各占半页。
 * 这样并发和错误注入都能聚焦在“访问时序”与“输入边界”，而不是新布局本身。
 */
#define DAY34_DMA_MASK_BITS_DEFAULT    32
#define DAY34_DMA_BYTES                4096
#define DAY34_DMA_SRC_OFF              0
#define DAY34_DMA_DST_OFF              2048
#define DAY34_DMA_VERIFY_MAX           2048

struct day34_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    /*
     * day34 的模块循环会频繁执行 insmod/rmmod。为了避免 remove()
     * 阶段对 IRQ/MSI 资源做重复释放，这里显式记录“是否申请过向量/IRQ”。
     */
    unsigned int irq_vector;
    bool irq_vectors_allocated;
    bool irq_requested;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;

    void *dma_virt;
    dma_addr_t dma_handle;
    size_t dma_bytes;
    u32 dma_mask_bits;

    u64 total_run_calls;
    u64 total_run_ok;
    u64 total_run_fail;
    u64 last_run_ns;

    u32 last_run_len;
    u32 last_run_seed;
    s32 last_run_error;
    u32 last_run_ok;
    u32 last_irq_delta;
    u32 last_dma_cmd;

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
