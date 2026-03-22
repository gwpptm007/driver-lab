// SPDX-License-Identifier: GPL-2.0
/*
 * Day26 - EDU userspace-friendly tool driver
 *
 * 设计目标：
 * 1. 延续 Day25 的 EDU + MSI 中断实验；
 * 2. 把字符设备接口做得更“用户态友好”：
 *    - ioctl: 结构化读取静态信息和统计信息；
 *    - read : 直接读取一段文本状态快照；
 *    - write: 直接写一个整数触发一次 EDU 中断；
 * 3. 明确错误码：
 *    - 无效输入      -> -EINVAL
 *    - 输入过长      -> -E2BIG
 *    - 设备不可用    -> -ENODEV
 *    - 用户拷贝失败  -> -EFAULT
 *
 * Day26 相比 Day25 的核心增量，不在“能不能打通中断”，而在：
 * - 用户态接口是否更接近日常工具使用方式；
 * - 正向与负向路径能否都给出清晰、稳定、可归档的输出。
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day26_edu_tool.h"
#include "../include/day26_edu_uapi.h"

/*
 * 全局字符设备资源：
 * - g_day26_base_dev: alloc_chrdev_region 分配出来的 major/minor 基础号；
 * - g_day26_class   : 对应 /sys/class/day26_edu；
 * - g_day26_minor   : 支持将来扩展多个同类设备时分配不同 minor。
 */
static dev_t g_day26_base_dev;
static struct class *g_day26_class;
static atomic_t g_day26_minor = ATOMIC_INIT(0);

/* BAR0 上固定寄存器的 32 位读写封装。 */
static u32 day26_read32(struct day26_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static void day26_write32(struct day26_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

/*
 * 中断处理函数：
 * 1. 读 IRQ_STATUS；
 * 2. 若状态为 0，返回 IRQ_NONE；
 * 3. 更新驱动内部计数与最后一次状态；
 * 4. 向 IRQ_ACK 写回相同值完成 ACK。
 */
static irqreturn_t day26_irq_handler(int irq, void *opaque)
{
    struct day26_dev *d = opaque;
    unsigned long flags;
    u32 status;

    status = day26_read32(d, DAY26_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;

    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    day26_write32(d, DAY26_EDU_REG_IRQ_ACK, status);

    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu\n",
             irq, status, d->irq_count);
    return IRQ_HANDLED;
}

/*
 * 生成 read() 用的文本状态快照。
 * 这样用户态不需要定义任何结构体，也能直接读到一段可复制到日志里的文本。
 */
static ssize_t day26_build_state_text(struct day26_dev *d, char *buf, size_t size)
{
    return scnprintf(buf, size,
                     "vendor=0x%04x device=0x%04x\n"
                     "bar0_start=0x%llx bar0_len=0x%llx\n"
                     "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
                     "identity_value=0x%08x\n"
                     "liveness_value=0x%08x liveness_inverted=0x%08x\n"
                     "last_irq_status=0x%08x last_ack_value=0x%08x\n",
                     d->pdev->vendor,
                     d->pdev->device,
                     (unsigned long long)d->bar0_start,
                     (unsigned long long)d->bar0_len,
                     d->irq_vector,
                     d->irq_count,
                     !!(d->pdev->msi_enabled),
                     d->identity_value,
                     d->liveness_value,
                     d->liveness_inverted,
                     d->last_irq_status,
                     d->last_ack_value);
}

static int day26_open(struct inode *inode, struct file *file)
{
    struct day26_dev *d = container_of(inode->i_cdev, struct day26_dev, cdev);
    file->private_data = d;
    return 0;
}

/* read()：返回一段文本状态，而不是二进制结构体。 */
static ssize_t day26_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct day26_dev *d = file->private_data;
    char kbuf[256];
    ssize_t len;

    if (!d || !d->bar0)
        return -ENODEV;

    len = day26_build_state_text(d, kbuf, sizeof(kbuf));
    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * write() 输入格式："1"、"0x1"、"0x5\n"。
 * Day26 故意要求“非 0”触发值，这样 guest 可以额外做一条负向测试：
 * `trigger 0` 应返回 -EINVAL，并在用户态工具里表现为 “Invalid argument”。
 */
static ssize_t day26_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct day26_dev *d = file->private_data;
    char kbuf[32];
    char *end;
    unsigned long v;

    if (!d || !d->bar0)
        return -ENODEV;
    if (count == 0)
        return -EINVAL;
    if (count >= sizeof(kbuf))
        return -E2BIG;
    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    strim(kbuf);
    if (!kbuf[0])
        return -EINVAL;

    v = simple_strtoul(kbuf, &end, 0);
    if (end == kbuf || *end != '\0')
        return -EINVAL;
    if (v == 0 || v > 0xffffffffUL)
        return -EINVAL;

    dev_info(&d->pdev->dev, "write trigger: value=0x%08lx\n", v);
    day26_write32(d, DAY26_EDU_REG_IRQ_RAISE, (u32)v);
    *ppos += count;
    return count;
}

/* ioctl：适合结构化获取状态，便于用户态工具稳定解析。 */
static long day26_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day26_dev *d = file->private_data;

    switch (cmd) {
    case DAY26_IOC_GET_INFO: {
        struct day26_info info = {
            .tool_api_version = DAY26_TOOL_API_VERSION,
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .identity_value = d->identity_value,
            .liveness_value = d->liveness_value,
            .liveness_inverted = d->liveness_inverted,
            .msi_enabled = !!(d->pdev->msi_enabled),
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }
    case DAY26_IOC_GET_IRQ_COUNT: {
        struct day26_irq_count cnt = { .count = d->irq_count };
        if (copy_to_user((void __user *)arg, &cnt, sizeof(cnt)))
            return -EFAULT;
        return 0;
    }
    case DAY26_IOC_GET_IRQ_STATUS: {
        struct day26_irq_status st = {
            .irq_status = d->last_irq_status,
            .ack_value = d->last_ack_value,
        };
        if (copy_to_user((void __user *)arg, &st, sizeof(st)))
            return -EFAULT;
        return 0;
    }
    case DAY26_IOC_RESET_STATS:
        d->irq_count = 0;
        d->last_irq_status = 0;
        d->last_ack_value = 0;
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations day26_fops = {
    .owner = THIS_MODULE,
    .open = day26_open,
    .read = day26_read,
    .write = day26_write,
    .unlocked_ioctl = day26_ioctl,
    .llseek = default_llseek,
};

/* 建立 /dev/day26_edu0 对应的字符设备节点。 */
static int day26_setup_chrdev(struct day26_dev *d)
{
    int ret;
    int minor = atomic_fetch_add(1, &g_day26_minor);

    d->devt = MKDEV(MAJOR(g_day26_base_dev), minor);
    cdev_init(&d->cdev, &day26_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    d->device = device_create(g_day26_class, &d->pdev->dev, d->devt, d,
                              DAY26_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }
    return 0;
}

static void day26_destroy_chrdev(struct day26_dev *d)
{
    if (d->device)
        device_destroy(g_day26_class, d->devt);
    cdev_del(&d->cdev);
}

static int day26_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day26_dev *d;
    int ret;

    dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->pdev = pdev;
    spin_lock_init(&d->irq_lock);
    pci_set_drvdata(pdev, d);

    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
        goto err_free;
    }

    ret = pci_request_regions(pdev, DAY26_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
        goto err_disable;
    }

    pci_set_master(pdev);

    d->bar0_start = pci_resource_start(pdev, DAY26_BAR0);
    d->bar0_len = pci_resource_len(pdev, DAY26_BAR0);
    d->bar0 = pci_iomap(pdev, DAY26_BAR0, 0);
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "pci_iomap BAR0 failed\n");
        goto err_regions;
    }

    /* Day26 继续沿用 Day25 的 EDU + MSI 模型：只申请 1 个 MSI vector。 */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors(MSI) failed: %d\n", ret);
        goto err_iounmap;
    }

    d->irq_vector = pci_irq_vector(pdev, 0);
    ret = request_irq(d->irq_vector, day26_irq_handler, 0, DAY26_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq failed: %d\n", ret);
        goto err_free_vectors;
    }

    /* 读两个固定寄存器做只读探测，方便后续 ioctl/read 输出。 */
    d->identity_value = day26_read32(d, DAY26_EDU_REG_ID);
    d->liveness_value = day26_read32(d, DAY26_EDU_REG_LIVENESS);
    d->liveness_inverted = ~d->liveness_value;

    dev_info(&pdev->dev, "BAR0: start=0x%llx len=0x%llx\n",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len);
    dev_info(&pdev->dev, "identity=0x%08x liveness=0x%08x inverted=0x%08x\n",
             d->identity_value, d->liveness_value, d->liveness_inverted);
    dev_info(&pdev->dev, "MSI vector=%d\n", d->irq_vector);

    ret = day26_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "setup chrdev failed: %d\n", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success\n");
    return 0;

err_irq:
    free_irq(d->irq_vector, d);
err_free_vectors:
    pci_free_irq_vectors(pdev);
err_iounmap:
    pci_iounmap(pdev, d->bar0);
err_regions:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free:
    pci_set_drvdata(pdev, NULL);
    kfree(d);
    return ret;
}

static void day26_remove(struct pci_dev *pdev)
{
    struct day26_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter\n");
    day26_destroy_chrdev(d);
    free_irq(d->irq_vector, d);
    pci_free_irq_vectors(pdev);
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    pci_set_drvdata(pdev, NULL);
    dev_info(&pdev->dev, "remove leave\n");
    kfree(d);
}

static const struct pci_device_id day26_ids[] = {
    { PCI_DEVICE(DAY26_EDU_VENDOR_ID, DAY26_EDU_DEVICE_ID) },
    { }
};
MODULE_DEVICE_TABLE(pci, day26_ids);

static struct pci_driver day26_pci_driver = {
    .name = DAY26_DRV_NAME,
    .id_table = day26_ids,
    .probe = day26_probe,
    .remove = day26_remove,
};

static int __init day26_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_day26_base_dev, 0, 32, DAY26_DRV_NAME);
    if (ret)
        return ret;

    g_day26_class = class_create(THIS_MODULE, DAY26_CLASS_NAME);
    if (IS_ERR(g_day26_class)) {
        unregister_chrdev_region(g_day26_base_dev, 32);
        return PTR_ERR(g_day26_class);
    }

    ret = pci_register_driver(&day26_pci_driver);
    if (ret) {
        class_destroy(g_day26_class);
        unregister_chrdev_region(g_day26_base_dev, 32);
        return ret;
    }

    pr_info(DAY26_DRV_NAME ": module init\n");
    return 0;
}

static void __exit day26_exit(void)
{
    pci_unregister_driver(&day26_pci_driver);
    class_destroy(g_day26_class);
    unregister_chrdev_region(g_day26_base_dev, 32);
    pr_info(DAY26_DRV_NAME ": module exit\n");
}

module_init(day26_init);
module_exit(day26_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day26 EDU userspace-friendly tool driver");
