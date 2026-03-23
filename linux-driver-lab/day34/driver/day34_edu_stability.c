// SPDX-License-Identifier: GPL-2.0
/*
 * Day34 - QEMU EDU stability baseline driver
 *
 * 这版驱动延续 day33 已跑通的 coherent DMA + mmap + RUN_DMA 基线，
 * 但把目标从 trace/bench 转移到稳定性场景：
 * 1. 多进程并发访问同一个字符设备；
 * 2. insmod/rmmod 生命周期循环；
 * 3. 非法长度与非法 mmap offset 错误注入。
 *
 * 因此驱动本身不再追求复杂输出，而是提供：
 * - 稳定的 mmap + RUN_DMA 数据路径；
 * - 可回读的最近一次运行结果；
 * - 明确的边界拒绝。
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>

#include "include/day34_edu_stability.h"
#include "../include/day34_edu_uapi.h"

static dev_t g_day34_base_dev;
static struct class *g_day34_class;
static atomic_t g_day34_minor = ATOMIC_INIT(0);

static unsigned int dma_mask_bits = DAY34_DMA_MASK_BITS_DEFAULT;
module_param(dma_mask_bits, uint, 0644);
MODULE_PARM_DESC(dma_mask_bits, "DMA mask bits for QEMU EDU (default 32)");

static bool stability_verbose;
module_param(stability_verbose, bool, 0644);
MODULE_PARM_DESC(stability_verbose, "Enable verbose hot-path logging (default false)");

static inline u32 day34_read32(struct day34_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static inline void day34_write32(struct day34_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

static inline void day34_write64(struct day34_dev *d, u32 off, u64 val)
{
    writeq(val, d->bar0 + off);
}

static irqreturn_t day34_irq_handler(int irq, void *opaque)
{
    struct day34_dev *d = opaque;
    unsigned long flags;
    u32 status;

    status = day34_read32(d, DAY34_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;

    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    day34_write32(d, DAY34_EDU_REG_IRQ_ACK, status);
    if (unlikely(stability_verbose))
        dev_info(&d->pdev->dev, "irq handler: irq=%d status=0x%08x count=%llu", irq, status, d->irq_count);
    return IRQ_HANDLED;
}

static int day34_wait_dma_idle(struct day34_dev *d)
{
    int i;
    u32 cmd;

    for (i = 0; i < 50000; ++i) {
        cmd = day34_read32(d, DAY34_EDU_REG_DMA_CMD);
        if (!(cmd & DAY34_EDU_DMA_CMD_START))
            return 0;
        udelay(10);
    }
    dev_err(&d->pdev->dev, "DMA timeout: cmd=0x%08x", day34_read32(d, DAY34_EDU_REG_DMA_CMD));
    return -ETIMEDOUT;
}

static int day34_program_dma(struct day34_dev *d, u64 src, u64 dst, u32 count, u32 cmd)
{
    if (!count)
        return -EINVAL;
    d->last_dma_cmd = cmd;
    day34_write64(d, DAY34_EDU_REG_DMA_SRC, src);
    day34_write64(d, DAY34_EDU_REG_DMA_DST, dst);
    day34_write32(d, DAY34_EDU_REG_DMA_COUNT, count);
    day34_write32(d, DAY34_EDU_REG_DMA_CMD, cmd);
    return day34_wait_dma_idle(d);
}

static void day34_reset_run_result(struct day34_dev *d)
{
    d->last_run_ns = 0;
    d->last_run_len = 0;
    d->last_run_seed = 0;
    d->last_run_error = 0;
    d->last_run_ok = 0;
    d->last_irq_delta = 0;
    d->last_dma_cmd = 0;
}

/*
 * Day34 会在最后做 fault-mmap-offset 注入，因此这里记录的 mmap 结果既用于
 * 正常路径，也用于错误路径留证。用户态读取 result 时，看到的就是“最近一
 * 次 mmap 尝试”的结果，而不是整轮回归统计。
 */
static void day34_record_mmap_result(struct day34_dev *d, bool ok, int err,
                                     unsigned long len, unsigned long pgoff)
{
    d->last_mmap_ok = ok ? 1U : 0U;
    d->last_mmap_error = err;
    d->last_mmap_len = (u32)len;
    d->last_mmap_pgoff = (u32)pgoff;
}

static int day34_do_run_dma(struct day34_dev *d, u32 len, u32 seed)
{
    u64 src_dma, dst_dma, irq_before, start_ns, end_ns;
    int ret;
    u8 *src, *dst;
    u32 i;

    if (!d->dma_virt)
        return -ENODEV;
    if (!len || len > DAY34_DMA_VERIFY_MAX)
        return -EINVAL;

    mutex_lock(&d->op_lock);
    day34_reset_run_result(d);
    d->total_run_calls++;
    d->last_run_len = len;
    d->last_run_seed = seed;

    src = (u8 *)d->dma_virt + DAY34_DMA_SRC_OFF;
    dst = (u8 *)d->dma_virt + DAY34_DMA_DST_OFF;
    for (i = 0; i < len; ++i)
        src[i] = (u8)((seed + i) & 0xff);
    memset(dst, 0, len);

    src_dma = (u64)d->dma_handle + DAY34_DMA_SRC_OFF;
    dst_dma = (u64)d->dma_handle + DAY34_DMA_DST_OFF;

    irq_before = d->irq_count;
    start_ns = ktime_get_ns();
    if (unlikely(stability_verbose))
        dev_info(&d->pdev->dev, "run_dma start: len=%u seed=0x%x src_dma=0x%llx dst_dma=0x%llx",
                 len, seed, src_dma, dst_dma);

    ret = day34_program_dma(d, src_dma, DAY34_EDU_DEVBUF_OFFSET, len, DAY34_EDU_DMA_CMD_START | DAY34_EDU_DMA_CMD_IRQ);
    if (ret)
        goto out;
    ret = day34_program_dma(d, DAY34_EDU_DEVBUF_OFFSET, dst_dma, len,
                            DAY34_EDU_DMA_CMD_START | DAY34_EDU_DMA_CMD_DIR_TO_RAM | DAY34_EDU_DMA_CMD_IRQ);
    if (ret)
        goto out;

    d->last_irq_delta = (u32)(d->irq_count - irq_before);
    d->last_run_ok = 1;
    d->total_run_ok++;
out:
    end_ns = ktime_get_ns();
    d->last_run_ns = end_ns - start_ns;
    d->last_run_error = ret;
    if (ret)
        d->total_run_fail++;
    if (unlikely(stability_verbose && !ret))
        dev_info(&d->pdev->dev, "run_dma ok: len=%u seed=0x%x irq_delta=%u", len, seed, d->last_irq_delta);
    mutex_unlock(&d->op_lock);
    return ret;
}

static int day34_open(struct inode *inode, struct file *filp)
{
    struct day34_dev *d = container_of(inode->i_cdev, struct day34_dev, cdev);
    filp->private_data = d;
    return 0;
}

static int day34_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t day34_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct day34_dev *d = filp->private_data;
    char tmp[512];
    int n;

    n = scnprintf(tmp, sizeof(tmp),
                  "vendor=0x%04x"
                  "device=0x%04x"
                  "irq_count=%llu"
                  "last_irq_status=0x%08x"
                  "dma_handle=0x%llx"
                  "dma_bytes=%zu"
                  "dma_mask_bits=%u"
                  "total_run_calls=%llu"
                  "total_run_ok=%llu"
                  "total_run_fail=%llu"
                  "last_run_len=%u"
                  "last_run_seed=0x%x"
                  "last_run_ok=%u"
                  "last_run_error=%d"
                  "last_irq_delta=%u"
                  "last_mmap_ok=%u"
                  "last_mmap_error=%d"
                  "last_mmap_len=%u"
                  "last_mmap_pgoff=%u",
                  d->pdev->vendor, d->pdev->device, d->irq_count,
                  d->last_irq_status, (unsigned long long)d->dma_handle,
                  d->dma_bytes, d->dma_mask_bits, d->total_run_calls,
                  d->total_run_ok, d->total_run_fail, d->last_run_len,
                  d->last_run_seed, d->last_run_ok, d->last_run_error,
                  d->last_irq_delta, d->last_mmap_ok, d->last_mmap_error,
                  d->last_mmap_len, d->last_mmap_pgoff);
    return simple_read_from_buffer(buf, len, ppos, tmp, n);
}

static long day34_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct day34_dev *d = filp->private_data;

    switch (cmd) {
    case DAY34_IOC_GET_INFO: {
        struct day34_info info = {0};
        info.tool_api_version = DAY34_TOOL_API_VERSION;
        info.vendor_id = d->pdev->vendor;
        info.device_id = d->pdev->device;
        info.irq_vector = d->irq_vector;
        info.irq_count = d->irq_count;
        info.last_irq_status = d->last_irq_status;
        info.last_ack_value = d->last_ack_value;
        info.bar0_start = d->bar0_start;
        info.bar0_len = d->bar0_len;
        info.dma_handle = d->dma_handle;
        info.dma_bytes = d->dma_bytes;
        info.dma_mask_bits = d->dma_mask_bits;
        info.msi_enabled = 1;
        info.map_bytes = PAGE_ALIGN(d->dma_bytes);
        info.src_off = DAY34_DMA_SRC_OFF;
        info.dst_off = DAY34_DMA_DST_OFF;
        info.max_verify_len = DAY34_DMA_VERIFY_MAX;
        info.total_run_calls = d->total_run_calls;
        info.total_run_ok = d->total_run_ok;
        info.total_run_fail = d->total_run_fail;
        info.last_run_ns = d->last_run_ns;
        info.last_run_len = d->last_run_len;
        info.last_run_seed = d->last_run_seed;
        info.last_run_ok = d->last_run_ok;
        info.last_run_error = d->last_run_error;
        info.last_irq_delta = d->last_irq_delta;
        info.last_dma_cmd = d->last_dma_cmd;
        info.last_mmap_ok = d->last_mmap_ok;
        info.last_mmap_error = d->last_mmap_error;
        info.last_mmap_len = d->last_mmap_len;
        info.last_mmap_pgoff = d->last_mmap_pgoff;
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }
    case DAY34_IOC_RUN_DMA: {
        struct day34_run_req req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;
        return day34_do_run_dma(d, req.len, req.pattern_seed);
    }
    case DAY34_IOC_GET_RESULT: {
        struct day34_run_result res = {0};
        res.total_run_calls = d->total_run_calls;
        res.total_run_ok = d->total_run_ok;
        res.total_run_fail = d->total_run_fail;
        res.last_run_ns = d->last_run_ns;
        res.run_len = d->last_run_len;
        res.run_seed = d->last_run_seed;
        res.run_ok = d->last_run_ok;
        res.run_error = d->last_run_error;
        res.irq_delta = d->last_irq_delta;
        res.last_dma_cmd = d->last_dma_cmd;
        res.mmap_ok = d->last_mmap_ok;
        res.mmap_error = d->last_mmap_error;
        res.mmap_len = d->last_mmap_len;
        res.mmap_pgoff = d->last_mmap_pgoff;
        if (copy_to_user((void __user *)arg, &res, sizeof(res)))
            return -EFAULT;
        return 0;
    }
    case DAY34_IOC_RESET_STATS:
        mutex_lock(&d->op_lock);
        d->total_run_calls = 0;
        d->total_run_ok = 0;
        d->total_run_fail = 0;
        day34_reset_run_result(d);
        day34_record_mmap_result(d, false, 0, 0, 0);
        mutex_unlock(&d->op_lock);
        return 0;
    default:
        return -ENOTTY;
    }
}

static int day34_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct day34_dev *d = filp->private_data;
    unsigned long len = vma->vm_end - vma->vm_start;

    if (vma->vm_pgoff != 0) {
        day34_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
        dev_err(&d->pdev->dev, "mmap rejected: pgoff=%lu must be 0", vma->vm_pgoff);
        return -EINVAL;
    }
    if (len != PAGE_ALIGN(d->dma_bytes)) {
        day34_record_mmap_result(d, false, -EINVAL, len, vma->vm_pgoff);
        dev_err(&d->pdev->dev, "mmap rejected: len=%lu expected=%lu", len, PAGE_ALIGN(d->dma_bytes));
        return -EINVAL;
    }
    if (dma_mmap_coherent(&d->pdev->dev, vma, d->dma_virt, d->dma_handle, d->dma_bytes)) {
        day34_record_mmap_result(d, false, -EAGAIN, len, vma->vm_pgoff);
        return -EAGAIN;
    }
    day34_record_mmap_result(d, true, 0, len, vma->vm_pgoff);
    return 0;
}

static const struct file_operations day34_fops = {
    .owner = THIS_MODULE,
    .open = day34_open,
    .release = day34_release,
    .read = day34_read,
    .unlocked_ioctl = day34_ioctl,
    .mmap = day34_mmap,
    .llseek = no_llseek,
};

static int day34_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day34_dev *d;
    int ret, minor;

    dev_info(&pdev->dev, "probe enter: %04x:%04x", pdev->vendor, pdev->device);

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    pci_set_drvdata(pdev, d);
    d->pdev = pdev;
    mutex_init(&d->op_lock);
    spin_lock_init(&d->irq_lock);
    d->dma_bytes = DAY34_DMA_BYTES;
    d->dma_mask_bits = dma_mask_bits;

    ret = pcim_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pcim_enable_device failed: %d", ret);
        return ret;
    }
    pci_set_master(pdev);

    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(d->dma_mask_bits));
    if (ret) {
        dev_err(&pdev->dev, "dma_set_mask_and_coherent(%u bits) failed: %d", d->dma_mask_bits, ret);
        return ret;
    }
    dev_info(&pdev->dev, "dma mask set to %u bits", d->dma_mask_bits);

    ret = pcim_iomap_regions(pdev, BIT(0), DAY34_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pcim_iomap_regions failed: %d", ret);
        return ret;
    }
    d->bar0 = pcim_iomap_table(pdev)[0];
    d->bar0_start = pci_resource_start(pdev, 0);
    d->bar0_len = pci_resource_len(pdev, 0);

    d->dma_virt = dma_alloc_coherent(&pdev->dev, d->dma_bytes, &d->dma_handle, GFP_KERNEL);
    if (!d->dma_virt) {
        dev_err(&pdev->dev, "dma_alloc_coherent failed");
        return -ENOMEM;
    }
    dev_info(&pdev->dev, "dma_alloc_coherent ok: dma_handle=0x%llx dma_bytes=%zu dma_mask_bits=%u",
             (unsigned long long)d->dma_handle, d->dma_bytes, d->dma_mask_bits);

    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
        goto err_dma;
    }
    d->irq_vectors_allocated = true;
    d->irq_vector = pci_irq_vector(pdev, 0);
    /*
     * 注意：这里故意不用 devm_request_irq()。
     *
     * day34 的模块循环会在 guest 中大量执行 rmmod。若使用 devm_request_irq()，
     * devres 释放发生在 remove() 返回之后；而 remove() 中如果先
     * pci_free_irq_vectors()，MSI 仍然被 IRQ 层引用，可能在 drivers/pci/msi.c
     * 的 free_msi_irqs() 路径上触发 BUG。
     *
     * 因此这里改成 request_irq()/free_irq() 手工配对，确保 remove() 阶段
     * 先释放 IRQ，再释放 MSI vectors。
     */
    ret = request_irq(d->irq_vector, day34_irq_handler, 0, DAY34_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq failed: %d\n", ret);
        goto err_irq_vectors;
    }
    d->irq_requested = true;

    minor = atomic_fetch_add_unless(&g_day34_minor, 1, INT_MAX);
    d->devt = MKDEV(MAJOR(g_day34_base_dev), minor);
    cdev_init(&d->cdev, &day34_fops);
    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        goto err_irq_vectors;
    d->device = device_create(g_day34_class, &pdev->dev, d->devt, d, DAY34_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        cdev_del(&d->cdev);
        goto err_irq_vectors;
    }

    dev_info(&pdev->dev, "probe success: dev=/dev/%s dma_handle=0x%llx bar0=[0x%pa + 0x%pa)",
             dev_name(d->device), (unsigned long long)d->dma_handle, &d->bar0_start, &d->bar0_len);
    return 0;

err_irq_vectors:
    if (d->irq_requested) {
        free_irq(d->irq_vector, d);
        d->irq_requested = false;
    }
    if (d->irq_vectors_allocated) {
        pci_free_irq_vectors(pdev);
        d->irq_vectors_allocated = false;
    }
err_dma:
    dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
    return ret;
}

static void day34_remove(struct pci_dev *pdev)
{
    struct day34_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;
    if (d->device)
        device_destroy(g_day34_class, d->devt);
    cdev_del(&d->cdev);
    /*
     * remove() 中的释放顺序要和申请顺序相反：IRQ -> MSI vectors -> DMA。
     * Day34 的 1000 次模块循环就是专门验证这里的释放路径是否长期稳定。
     */
    if (d->irq_requested) {
        free_irq(d->irq_vector, d);
        d->irq_requested = false;
    }
    if (d->irq_vectors_allocated) {
        pci_free_irq_vectors(pdev);
        d->irq_vectors_allocated = false;
    }
    if (d->dma_virt)
        dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
    dev_info(&pdev->dev, "remove complete");
}

static const struct pci_device_id day34_ids[] = {
    { PCI_DEVICE(DAY34_EDU_VENDOR_ID, DAY34_EDU_DEVICE_ID) },
    { }
};
MODULE_DEVICE_TABLE(pci, day34_ids);

static struct pci_driver day34_pci_driver = {
    .name = DAY34_DRV_NAME,
    .id_table = day34_ids,
    .probe = day34_probe,
    .remove = day34_remove,
};

static int __init day34_init(void)
{
    int ret;
    ret = alloc_chrdev_region(&g_day34_base_dev, 0, 256, DAY34_DRV_NAME);
    if (ret)
        return ret;
    g_day34_class = class_create(THIS_MODULE, DAY34_CLASS_NAME);
    if (IS_ERR(g_day34_class)) {
        unregister_chrdev_region(g_day34_base_dev, 256);
        return PTR_ERR(g_day34_class);
    }
    ret = pci_register_driver(&day34_pci_driver);
    if (ret) {
        class_destroy(g_day34_class);
        unregister_chrdev_region(g_day34_base_dev, 256);
        return ret;
    }
    pr_info("%s: init ok", DAY34_DRV_NAME);
    return 0;
}

static void __exit day34_exit(void)
{
    pci_unregister_driver(&day34_pci_driver);
    class_destroy(g_day34_class);
    unregister_chrdev_region(g_day34_base_dev, 256);
    pr_info("%s: exit ok", DAY34_DRV_NAME);
}

module_init(day34_init);
module_exit(day34_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day34 stability EDU driver");
