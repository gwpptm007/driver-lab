// SPDX-License-Identifier: GPL-2.0
/*
 * Day29 - QEMU EDU coherent DMA minimal round-trip
 *
 * 目标：
 * 1. 在 probe() 中显式设置 DMA mask；
 * 2. 使用 dma_alloc_coherent() 申请 4KB 一致性 DMA buffer；
 * 3. 通过 EDU DMA engine 完成两次搬运：
 *      RAM -> EDU internal buffer -> RAM
 * 4. 在驱动里比较搬运前后的内容，确认 round-trip 正确；
 * 5. 保留最小字符设备接口，方便 guest 工具做验证和取证。
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day29_edu_dma.h"
#include "../include/day29_edu_uapi.h"

static dev_t g_day29_base_dev;
static struct class *g_day29_class;
static atomic_t g_day29_minor = ATOMIC_INIT(0);

/*
 * Day29 这版自动化默认按 32-bit DMA mask 运行：
 * 1. QEMU 侧已经显式传入 -device edu,dma_mask=0xffffffff；
 * 2. arm64 virt 下 guest RAM 常常不落在 28-bit 可达窗口内；
 * 3. 早期版本通过 guest insmod 传 dma_mask_bits=32，但现场出现过
 *    模块参数传递链路不稳定、最终被内核解析为空值的情况。
 *
 * 因此这里把默认值直接收口为 32，自动化流程不再依赖 guest 运行时
 * 模块参数；仍然保留 module_param，方便后续手工实验时覆盖。
 */
static unsigned int dma_mask_bits = DAY29_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits, "DMA mask bits for QEMU EDU (default 32 in day29 automation; may be overridden manually for experiments)");

static inline u32 day29_read32(struct day29_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static inline void day29_write32(struct day29_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

static inline u64 day29_read64(struct day29_dev *d, u32 off)
{
    return readq(d->bar0 + off);
}

static inline void day29_write64(struct day29_dev *d, u32 off, u64 val)
{
    writeq(val, d->bar0 + off);
}

static irqreturn_t day29_irq_handler(int irq, void *opaque)
{
    struct day29_dev *d = opaque;
    unsigned long flags;
    u32 status;

    status = day29_read32(d, DAY29_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;

    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    day29_write32(d, DAY29_EDU_REG_IRQ_ACK, status);
    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu\n",
             irq, status, d->irq_count);
    return IRQ_HANDLED;
}

static int day29_wait_dma_idle(struct day29_dev *d)
{
    int i;
    u32 cmd;

    /*
     * 旧版这里轮询 10000 * 10us ~= 100ms。
     * 现场实测第二段 DMA 在极限情况下会稍慢于 100ms，导致“IRQ 已到，
     * 但仍被判成超时”的误判。因此这里放宽到 50000 * 10us ~= 500ms。
     */
    for (i = 0; i < 50000; ++i) {
        cmd = day29_read32(d, DAY29_EDU_REG_DMA_CMD);
        if (!(cmd & DAY29_EDU_DMA_CMD_START))
            return 0;
        udelay(10);
    }

    dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x\n",
            day29_read32(d, DAY29_EDU_REG_DMA_CMD));
    return -ETIMEDOUT;
}

static int day29_program_dma(struct day29_dev *d, u64 src, u64 dst,
                             u32 count, u32 cmd)
{
    if (!count)
        return -EINVAL;

    d->last_dma_cmd = cmd;
    day29_write64(d, DAY29_EDU_REG_DMA_SRC, src);
    day29_write64(d, DAY29_EDU_REG_DMA_DST, dst);
    day29_write32(d, DAY29_EDU_REG_DMA_COUNT, count);
    day29_write32(d, DAY29_EDU_REG_DMA_CMD, cmd);
    return day29_wait_dma_idle(d);
}

static void day29_fill_pattern(u8 *buf, u32 len, u32 seed)
{
    u32 i;

    for (i = 0; i < len; ++i)
        buf[i] = (u8)((seed + i) & 0xff);
}

static void day29_reset_verify_result(struct day29_dev *d)
{
    d->last_verify_len = 0;
    d->last_verify_seed = 0;
    d->last_verify_error = 0;
    d->last_verify_ok = 0;
    d->last_mismatch_index = -1;
    d->last_mismatch_expected = 0;
    d->last_mismatch_actual = 0;
    d->last_irq_delta = 0;
    d->last_dma_cmd = 0;
}

static int day29_do_verify(struct day29_dev *d, u32 len, u32 seed)
{
    u8 *src;
    u8 *dst;
    u64 src_dma;
    u64 dst_dma;
    u64 irq_before;
    int ret;
    u32 i;

    if (!d->dma_virt)
        return -ENODEV;
    if (!len || len > DAY29_DMA_VERIFY_MAX)
        return -EINVAL;

    mutex_lock(&d->op_lock);
    day29_reset_verify_result(d);
    d->last_verify_len = len;
    d->last_verify_seed = seed;

    src = (u8 *)d->dma_virt + DAY29_DMA_SRC_OFF;
    dst = (u8 *)d->dma_virt + DAY29_DMA_DST_OFF;
    src_dma = (u64)d->dma_handle + DAY29_DMA_SRC_OFF;
    dst_dma = (u64)d->dma_handle + DAY29_DMA_DST_OFF;

    memset(d->dma_virt, 0, d->dma_bytes);
    day29_fill_pattern(src, len, seed);
    memset(dst, 0, len);

    irq_before = d->irq_count;
    dev_info(&d->pdev->dev,
             "verify start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx\n",
             len, seed,
             (unsigned long long)src_dma,
             (unsigned long long)dst_dma);

    ret = day29_program_dma(d, src_dma, DAY29_EDU_DEVBUF_OFFSET,
                            len,
                            DAY29_EDU_DMA_CMD_START |
                            DAY29_EDU_DMA_CMD_IRQ);
    if (ret) {
        d->last_verify_error = ret;
        dev_err(&d->pdev->dev, "verify stage1 RAM->EDU failed: %d\n", ret);
        goto out;
    }

    ret = day29_program_dma(d, DAY29_EDU_DEVBUF_OFFSET, dst_dma,
                            len,
                            DAY29_EDU_DMA_CMD_START |
                            DAY29_EDU_DMA_CMD_DIR_TO_RAM |
                            DAY29_EDU_DMA_CMD_IRQ);
    if (ret) {
        d->last_verify_error = ret;
        dev_err(&d->pdev->dev, "verify stage2 EDU->RAM failed: %d\n", ret);
        goto out;
    }

    d->last_irq_delta = (u32)(d->irq_count - irq_before);

    for (i = 0; i < len; ++i) {
        if (src[i] != dst[i]) {
            d->last_verify_error = -EIO;
            d->last_mismatch_index = (s32)i;
            d->last_mismatch_expected = src[i];
            d->last_mismatch_actual = dst[i];
            dev_err(&d->pdev->dev,
                    "verify mismatch: idx=%u expected=0x%02x actual=0x%02x\n",
                    i, src[i], dst[i]);
            goto out;
        }
    }

    d->last_verify_ok = 1;
    dev_info(&d->pdev->dev,
             "verify ok: len=%u seed=0x%x irq_delta=%u\n",
             len, seed, d->last_irq_delta);

out:
    mutex_unlock(&d->op_lock);
    return d->last_verify_error;
}

static ssize_t day29_build_state_text(struct day29_dev *d, char *buf, size_t size)
{
    return scnprintf(buf, size,
                     "vendor=0x%04x device=0x%04x\n"
                     "bar0_start=0x%llx bar0_len=0x%llx\n"
                     "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
                     "last_irq_status=0x%08x last_ack_value=0x%08x\n"
                     "dma_handle=0x%llx dma_bytes=%zu dma_mask_bits=%u\n"
                     "verify_len=%u verify_seed=0x%x verify_ok=%u verify_error=%d\n"
                     "mismatch_index=%d mismatch_expected=0x%02x mismatch_actual=0x%02x\n"
                     "last_irq_delta=%u last_dma_cmd=0x%08x\n",
                     d->pdev->vendor,
                     d->pdev->device,
                     (unsigned long long)d->bar0_start,
                     (unsigned long long)d->bar0_len,
                     d->irq_vector,
                     d->irq_count,
                     !!(d->pdev->msi_enabled),
                     d->last_irq_status,
                     d->last_ack_value,
                     (unsigned long long)d->dma_handle,
                     d->dma_bytes,
                     d->dma_mask_bits,
                     d->last_verify_len,
                     d->last_verify_seed,
                     d->last_verify_ok,
                     d->last_verify_error,
                     d->last_mismatch_index,
                     d->last_mismatch_expected,
                     d->last_mismatch_actual,
                     d->last_irq_delta,
                     d->last_dma_cmd);
}

/*
 * 字符设备 open 很轻，只负责把 struct day29_dev 挂到 file->private_data。
 * Day29 的重点不在 open/release 语义，而在 read/ioctl 这两个取证入口。
 */
static int day29_open(struct inode *inode, struct file *file)
{
    struct day29_dev *d = container_of(inode->i_cdev, struct day29_dev, cdev);

    file->private_data = d;
    return 0;
}

/*
 * read() 返回一段可直接 cat 的状态文本。
 * 这样 guest init 里用 `cat /dev/day29_edu0` 就能把状态打进 serial.log，
 * 对学习和 records 归档都很友好。
 */
static ssize_t day29_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct day29_dev *d = file->private_data;
    char kbuf[320];
    ssize_t len;

    if (!d || !d->bar0)
        return -ENODEV;

    len = day29_build_state_text(d, kbuf, sizeof(kbuf));
    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * ioctl 是 Day29 的主操作面：
 * - GET_INFO：读取静态/动态状态
 * - RUN_VERIFY：触发一轮 DMA round-trip
 * - GET_VERIFY_RESULT：读取最近一次 verify 结果
 * - RESET_STATS：清空计数器，便于重复实验
 */
static long day29_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day29_dev *d = file->private_data;

    switch (cmd) {
    case DAY29_IOC_GET_INFO: {
        struct day29_info info = {
            .tool_api_version = DAY29_TOOL_API_VERSION,
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .dma_handle = d->dma_handle,
            .dma_bytes = d->dma_bytes,
            .dma_mask_bits = d->dma_mask_bits,
            .msi_enabled = !!(d->pdev->msi_enabled),
            .last_verify_len = d->last_verify_len,
            .last_verify_seed = d->last_verify_seed,
            .last_verify_ok = d->last_verify_ok,
            .last_verify_error = d->last_verify_error,
            .last_mismatch_index = d->last_mismatch_index,
            .last_mismatch_expected = d->last_mismatch_expected,
            .last_mismatch_actual = d->last_mismatch_actual,
            .last_irq_delta = d->last_irq_delta,
            .last_dma_cmd = d->last_dma_cmd,
        };

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }
    case DAY29_IOC_RUN_VERIFY: {
        struct day29_verify_req req;

        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        return day29_do_verify(d, req.len, req.pattern_seed);
    }
    case DAY29_IOC_GET_VERIFY_RESULT: {
        struct day29_verify_result res = {
            .verify_len = d->last_verify_len,
            .verify_seed = d->last_verify_seed,
            .verify_ok = d->last_verify_ok,
            .verify_error = d->last_verify_error,
            .mismatch_index = d->last_mismatch_index,
            .mismatch_expected = d->last_mismatch_expected,
            .mismatch_actual = d->last_mismatch_actual,
            .irq_delta = d->last_irq_delta,
            .last_dma_cmd = d->last_dma_cmd,
        };

        if (copy_to_user((void __user *)arg, &res, sizeof(res)))
            return -EFAULT;
        return 0;
    }
    case DAY29_IOC_RESET_STATS:
        mutex_lock(&d->op_lock);
        d->irq_count = 0;
        d->last_irq_status = 0;
        d->last_ack_value = 0;
        day29_reset_verify_result(d);
        mutex_unlock(&d->op_lock);
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations day29_fops = {
    .owner          = THIS_MODULE,
    .open           = day29_open,
    .read           = day29_read,
    .unlocked_ioctl = day29_ioctl,
    .llseek         = no_llseek,
};

/*
 * 建立 `/dev/day29_eduX`。
 *
 * Day29 保留字符设备而不是只做 dmesg 打印，是因为后续 day30 还要继续
 * 复用这个“用户态可触发、可读结果”的交互面。
 */
static int day29_setup_chrdev(struct day29_dev *d)
{
    int minor;
    int ret;

    minor = atomic_fetch_add(1, &g_day29_minor);
    d->devt = MKDEV(MAJOR(g_day29_base_dev), minor);

    cdev_init(&d->cdev, &day29_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    d->device = device_create(g_day29_class, &d->pdev->dev, d->devt, NULL,
                              DAY29_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }
    return 0;
}

static void day29_destroy_chrdev(struct day29_dev *d)
{
    if (d->device)
        device_destroy(g_day29_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * probe 是 Day29 最核心的 bring-up 路径：
 * 1. 使能 PCI 设备
 * 2. 设置 DMA mask
 * 3. 申请 BAR / ioremap
 * 4. 申请 coherent DMA buffer
 * 5. 建立 IRQ
 * 6. 建立字符设备
 */
static int day29_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day29_dev *d;
    u32 ident;
    u32 live;
    int ret;

    dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    d->pdev = pdev;
    /*
     * 默认用模块参数给出的 mask bits。Day29 自动化默认值已经是 32，
     * 因此 guest init 不再额外传 dma_mask_bits=32，避免运行时参数链路
     * 干扰自动验收。
     */
    d->dma_mask_bits = dma_mask_bits;
    d->dma_bytes = DAY29_DMA_BYTES;
    d->last_mismatch_index = -1;
    spin_lock_init(&d->irq_lock);
    mutex_init(&d->op_lock);
    pci_set_drvdata(pdev, d);

    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
        goto err_free;
    }

    ret = dma_set_mask_and_coherent(&pdev->dev,
                                    DMA_BIT_MASK(d->dma_mask_bits));
    if (ret) {
        dev_err(&pdev->dev,
                "dma_set_mask_and_coherent(%u bits) failed: %d\n",
                d->dma_mask_bits, ret);
        goto err_disable;
    }
    dev_info(&pdev->dev, "dma mask set to %u bits\n", d->dma_mask_bits);

    ret = pci_request_regions(pdev, DAY29_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
        goto err_disable;
    }

    pci_set_master(pdev);

    d->bar0_start = pci_resource_start(pdev, 0);
    d->bar0_len = pci_resource_len(pdev, 0);
    dev_info(&pdev->dev, "BAR0: start=0x%llx len=0x%llx flags=0x%lx\n",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len,
             (unsigned long)pci_resource_flags(pdev, 0));

    d->bar0 = pci_iomap(pdev, 0, 0);
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "BAR0: pci_iomap failed\n");
        goto err_regions;
    }

    ident = day29_read32(d, DAY29_EDU_REG_IDENTITY);
    live = day29_read32(d, DAY29_EDU_REG_LIVENESS);
    dev_info(&pdev->dev, "ident=0x%08x liveness=0x%08x\n", ident, live);

    d->dma_virt = dma_alloc_coherent(&pdev->dev, d->dma_bytes,
                                     &d->dma_handle, GFP_KERNEL);
    if (!d->dma_virt) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "dma_alloc_coherent(%zu) failed\n", d->dma_bytes);
        goto err_iounmap;
    }
    memset(d->dma_virt, 0, d->dma_bytes);
    dev_info(&pdev->dev,
             "dma_alloc_coherent ok: virt=%px dma=0x%llx bytes=%zu\n",
             d->dma_virt,
             (unsigned long long)d->dma_handle,
             d->dma_bytes);

    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
        goto err_dma;
    }
    d->irq_vector = pci_irq_vector(pdev, 0);

    ret = request_irq(d->irq_vector, day29_irq_handler, 0, DAY29_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq(%u) failed: %d\n", d->irq_vector, ret);
        goto err_irq_vectors;
    }
    dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
             d->irq_vector, !!pdev->msi_enabled);

    ret = day29_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "day29_setup_chrdev failed: %d\n", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success\n");
    return 0;

err_irq:
    free_irq(d->irq_vector, d);
err_irq_vectors:
    pci_free_irq_vectors(pdev);
err_dma:
    dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
err_iounmap:
    pci_iounmap(pdev, d->bar0);
err_regions:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free:
    kfree(d);
    return ret;
}

/*
 * remove 与 probe 严格反向拆除，避免下一轮 insmod / rmmod 残留状态。
 */
static void day29_remove(struct pci_dev *pdev)
{
    struct day29_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter\n");
    day29_destroy_chrdev(d);
    free_irq(d->irq_vector, d);
    pci_free_irq_vectors(pdev);
    if (d->dma_virt)
        dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    dev_info(&pdev->dev, "remove leave\n");
    kfree(d);
}

static const struct pci_device_id day29_pci_ids[] = {
    { PCI_DEVICE(DAY29_EDU_VENDOR_ID, DAY29_EDU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, day29_pci_ids);

static struct pci_driver day29_pci_driver = {
    .name = DAY29_DRV_NAME,
    .id_table = day29_pci_ids,
    .probe = day29_probe,
    .remove = day29_remove,
};

/*
 * 模块初始化先准备字符设备号和 class，再注册 PCI driver。
 * 这样一旦 probe 成功，就能立即创建设备节点。
 */
static int __init day29_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_day29_base_dev, 0, 256, DAY29_DRV_NAME);
    if (ret)
        return ret;

    g_day29_class = class_create(THIS_MODULE, DAY29_CLASS_NAME);
    if (IS_ERR(g_day29_class)) {
        ret = PTR_ERR(g_day29_class);
        unregister_chrdev_region(g_day29_base_dev, 256);
        return ret;
    }

    ret = pci_register_driver(&day29_pci_driver);
    if (ret) {
        class_destroy(g_day29_class);
        unregister_chrdev_region(g_day29_base_dev, 256);
        return ret;
    }

    pr_info(DAY29_DRV_NAME ": init ok\n");
    return 0;
}

static void __exit day29_exit(void)
{
    pci_unregister_driver(&day29_pci_driver);
    class_destroy(g_day29_class);
    unregister_chrdev_region(g_day29_base_dev, 256);
    pr_info(DAY29_DRV_NAME ": exit ok\n");
}

module_init(day29_init);
module_exit(day29_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day29 QEMU EDU coherent DMA round-trip driver");
MODULE_LICENSE("GPL");
