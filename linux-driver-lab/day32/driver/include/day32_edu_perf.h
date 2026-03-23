/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY32_EDU_PERF_H
#define DAY32_EDU_PERF_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#define DAY32_DRV_NAME                 "day32_edu_perf"
#define DAY32_CLASS_NAME               "day32_edu"
#define DAY32_DEV_NAME_FMT             "day32_edu%d"

#define DAY32_EDU_VENDOR_ID            0x1234
#define DAY32_EDU_DEVICE_ID            0x11e8

/* QEMU EDU MMIO register map used by Day32. */
#define DAY32_EDU_REG_IDENTITY         0x00
#define DAY32_EDU_REG_LIVENESS         0x04
#define DAY32_EDU_REG_IRQ_STATUS       0x24
#define DAY32_EDU_REG_IRQ_ACK          0x64
#define DAY32_EDU_REG_DMA_SRC          0x80
#define DAY32_EDU_REG_DMA_DST          0x88
#define DAY32_EDU_REG_DMA_COUNT        0x90
#define DAY32_EDU_REG_DMA_CMD          0x98

#define DAY32_EDU_DEVBUF_OFFSET        0x40000ULL

#define DAY32_EDU_DMA_CMD_START        0x01
#define DAY32_EDU_DMA_CMD_DIR_TO_RAM   0x02
#define DAY32_EDU_DMA_CMD_IRQ          0x04

/*
 * Day32 继续沿用 day30 的 DMA buffer 布局。
 * 这样 bench 的变量更少：
 * - src_off: 用户态写入 pattern 的位置
 * - dst_off: 用户态读取结果的位置
 * - max_verify_len: 为了让 src/dst 不重叠，最大验证长度仍然设为半页
 */
#define DAY32_DMA_MASK_BITS_DEFAULT    32
#define DAY32_DMA_BYTES                4096
#define DAY32_DMA_SRC_OFF              0
#define DAY32_DMA_DST_OFF              2048
#define DAY32_DMA_VERIFY_MAX           2048

struct day32_dev {
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

    /*
     * Day32 相比 day30 新增了“最小 bench 统计”：
     * - total_run_calls / ok / fail：让 guest 在 bench 结束后能快速看驱动侧总体行为
     * - last_run_ns：记录最近一次 RUN_DMA 从发起到等待完成的内核视角耗时
     */
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
