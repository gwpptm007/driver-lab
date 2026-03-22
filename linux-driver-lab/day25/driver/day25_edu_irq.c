// SPDX-License-Identifier: GPL-2.0
/*
 * Day25 - EDU MSI interrupt experiment
 *
 * 目标：
 * 1. 在 QEMU virt 平台上匹配 EDU 设备（1234:11e8）
 * 2. 申请 BAR0 并通过 readl/writel 访问 EDU 寄存器
 * 3. 显式申请 MSI vector，并注册中断处理函数
 * 4. 暴露一个最小字符设备给用户态工具，用于：
 *    - 查询设备/中断状态
 *    - 主动触发一次 EDU 中断
 *    - 读取中断计数和最后一次状态/ACK 值
 *
 * 设计原则：
 * - 只做 day25 所需的最小功能，不引入与 day26 以后才需要的复杂控制面。
 * - 所有实验状态都放在 struct day25_dev 中，便于 probe/remove 生命周期管理。
 */
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day25_edu_irq.h"
#include "../include/day25_edu_uapi.h"

/*
 * 整个模块共享一组字符设备主设备号和 class：
 * - g_day25_base_dev：alloc_chrdev_region() 分配到的主设备号
 * - g_day25_class：device_create() 依赖的 class，devtmpfs/sysfs 都会用到
 * - g_day25_minor：当前已经分配了多少个 minor。EDU 实验通常只有一个设备，
 *   但这里仍按通用方式实现，便于后续扩展到多设备场景。
 */
static dev_t g_day25_base_dev;
static struct class *g_day25_class;
static atomic_t g_day25_minor = ATOMIC_INIT(0);

/*
 * EDU 寄存器都是 MMIO 32-bit 寄存器。
 * 这里单独封装 read32/write32，方便：
 * - 统一偏移语义（off 是 BAR0 内偏移）
 * - 后续加 trace/log 时不需要改很多调用点
 */
static u32 day25_read32(struct day25_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static void day25_write32(struct day25_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

/*
 * EDU 中断处理函数：
 * 1. 先读 IRQ_STATUS，0 表示这次中断不是我们关心的设备事件
 * 2. 在自旋锁保护下更新 irq_count / last_irq_status / last_ack_value
 * 3. 向 IRQ_ACK 写同样的值完成 ACK
 *
 * 注意：这里既维护“计数”也维护“最后一次状态”，因为 day25 用户态工具需要
 * 在触发前后读取这些状态，形成一条可验证的证据链。
 */
static irqreturn_t day25_irq_handler(int irq, void *opaque)
{
    struct day25_dev *d = opaque;
    unsigned long flags;
    u32 status;

    status = day25_read32(d, DAY25_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;

    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    day25_write32(d, DAY25_EDU_REG_IRQ_ACK, status);

    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu",
             irq, status, d->irq_count);
    return IRQ_HANDLED;
}

/*
 * open 非常简单：把 struct day25_dev 放到 file->private_data，
 * 后续 ioctl 直接从这里拿设备上下文。
 */
static int day25_open(struct inode *inode, struct file *file)
{
    struct day25_dev *d = container_of(inode->i_cdev, struct day25_dev, cdev);
    file->private_data = d;
    return 0;
}

/*
 * ioctl 是 day25 用户态验证的主要控制入口：
 * - GET_INFO：读取 probe 阶段得到的静态信息 + 当前中断状态
 * - TRIGGER_IRQ：往 EDU 的 IRQ_RAISE 寄存器写值，触发一次中断
 * - GET_IRQ_COUNT：获取驱动内累计中断次数
 * - GET_IRQ_STATUS：获取最近一次中断状态值和 ACK 值
 */
static long day25_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day25_dev *d = file->private_data;

    switch (cmd) {
    case DAY25_IOC_GET_INFO: {
        struct day25_info info = {
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .liveness_value = d->liveness_value,
            .liveness_inverted = d->liveness_inverted,
            .msi_enabled = !!(d->pdev->msi_enabled),
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }
    case DAY25_IOC_TRIGGER_IRQ: {
        struct day25_trigger trig;
        if (copy_from_user(&trig, (void __user *)arg, sizeof(trig)))
            return -EFAULT;
        dev_info(&d->pdev->dev, "trigger irq: value=0x%08x", trig.value);
        day25_write32(d, DAY25_EDU_REG_IRQ_RAISE, trig.value);
        return 0;
    }
    case DAY25_IOC_GET_IRQ_COUNT: {
        struct day25_irq_count cnt = { .count = d->irq_count };
        if (copy_to_user((void __user *)arg, &cnt, sizeof(cnt)))
            return -EFAULT;
        return 0;
    }
    case DAY25_IOC_GET_IRQ_STATUS: {
        struct day25_irq_status st = {
            .irq_status = d->last_irq_status,
            .ack_value = d->last_ack_value,
        };
        if (copy_to_user((void __user *)arg, &st, sizeof(st)))
            return -EFAULT;
        return 0;
    }
    default:
        return -ENOTTY;
    }
}

static const struct file_operations day25_fops = {
    .owner = THIS_MODULE,
    .open = day25_open,
    .unlocked_ioctl = day25_ioctl,
    .llseek = no_llseek,
};

/*
 * probe 成功后为当前 EDU 设备建立字符设备节点。
 * 这一步的结果最终会映射到：
 * - /sys/class/day25_edu/day25_edu0/
 * - （若 devtmpfs 可用）/dev/day25_edu0
 *
 * 这也是 day25 用户态工具能否真正打开设备的关键。
 */
static int day25_setup_chrdev(struct day25_dev *d)
{
    int ret;
    int minor = atomic_fetch_add(1, &g_day25_minor);

    d->devt = MKDEV(MAJOR(g_day25_base_dev), minor);
    cdev_init(&d->cdev, &day25_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    d->device = device_create(g_day25_class, &d->pdev->dev, d->devt, d,
                              DAY25_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }

    return 0;
}

static void day25_destroy_chrdev(struct day25_dev *d)
{
    if (d->device)
        device_destroy(g_day25_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * probe 顺序：
 * 1. 使能 PCI 设备
 * 2. 申请 BAR 资源
 * 3. ioremap BAR0
 * 4. 申请 MSI vector
 * 5. request_irq 注册中断处理函数
 * 6. 读取 EDU identity / liveness 寄存器，证明 MMIO 可访问
 * 7. 建立字符设备节点，给 day25_irq_tool 使用
 */
static int day25_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day25_dev *d;
    int ret;
    u32 ident;
    u32 live;

    dev_info(&pdev->dev, "probe enter: %04x:%04x", pdev->vendor, pdev->device);

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->pdev = pdev;
    spin_lock_init(&d->irq_lock);
    pci_set_drvdata(pdev, d);

    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d", ret);
        goto err_free;
    }

    ret = pci_request_regions(pdev, DAY25_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d", ret);
        goto err_disable;
    }

    pci_set_master(pdev);

    d->bar0_start = pci_resource_start(pdev, DAY25_BAR0);
    d->bar0_len = pci_resource_len(pdev, DAY25_BAR0);
    d->bar0 = pci_iomap(pdev, DAY25_BAR0, 0);
    if (!d->bar0) {
        ret = -ENOMEM;
        dev_err(&pdev->dev, "pci_iomap BAR0 failed");
        goto err_regions;
    }

    /*
     * Day25 明确要求走 MSI，因此这里不做 INTx 兜底。
     * 如果 pci_alloc_irq_vectors() 失败，直接视为 day25 失败。
     */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors(MSI) failed: %d", ret);
        goto err_iounmap;
    }

    d->irq_vector = pci_irq_vector(pdev, 0);
    ret = request_irq(d->irq_vector, day25_irq_handler, 0, DAY25_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq failed: %d", ret);
        goto err_irq_vectors;
    }

    ident = day25_read32(d, DAY25_EDU_REG_ID);
    live = 0xa5a55a5a;
    day25_write32(d, DAY25_EDU_REG_LIVENESS, live);
    d->liveness_value = live;
    d->liveness_inverted = day25_read32(d, DAY25_EDU_REG_LIVENESS);

    dev_info(&pdev->dev,
             "BAR0: start=0x%llx len=0x%llx flags=0x%llx",
             (unsigned long long)d->bar0_start,
             (unsigned long long)d->bar0_len,
             (unsigned long long)pci_resource_flags(pdev, DAY25_BAR0));
    dev_info(&pdev->dev,
             "MSI vector=%d ident=0x%08x liveness=0x%08x inverted=0x%08x",
             d->irq_vector, ident, d->liveness_value, d->liveness_inverted);

    ret = day25_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "setup chrdev failed: %d", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success");
    return 0;

err_irq:
    free_irq(d->irq_vector, d);
err_irq_vectors:
    pci_free_irq_vectors(pdev);
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
 * remove 与 probe 保持严格对称：
 * - 销毁字符设备
 * - free_irq / pci_free_irq_vectors
 * - unmap BAR0
 * - release_regions / disable_device
 */
static void day25_remove(struct pci_dev *pdev)
{
    struct day25_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter");
    day25_destroy_chrdev(d);
    free_irq(d->irq_vector, d);
    pci_free_irq_vectors(pdev);
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    kfree(d);
    dev_info(&pdev->dev, "remove leave");
}

static const struct pci_device_id day25_ids[] = {
    { PCI_DEVICE(DAY25_EDU_VENDOR_ID, DAY25_EDU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, day25_ids);

static struct pci_driver day25_pci_driver = {
    .name = DAY25_DRV_NAME,
    .id_table = day25_ids,
    .probe = day25_probe,
    .remove = day25_remove,
};

static int __init day25_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_day25_base_dev, 0, DAY25_MAX_MINORS, DAY25_DRV_NAME);
    if (ret)
        return ret;

    g_day25_class = class_create(THIS_MODULE, DAY25_CLASS_NAME);
    if (IS_ERR(g_day25_class)) {
        ret = PTR_ERR(g_day25_class);
        g_day25_class = NULL;
        unregister_chrdev_region(g_day25_base_dev, DAY25_MAX_MINORS);
        return ret;
    }

    ret = pci_register_driver(&day25_pci_driver);
    if (ret) {
        class_destroy(g_day25_class);
        g_day25_class = NULL;
        unregister_chrdev_region(g_day25_base_dev, DAY25_MAX_MINORS);
        return ret;
    }

    pr_info("day25_edu_irq: module loaded");
    return 0;
}

static void __exit day25_exit(void)
{
    pci_unregister_driver(&day25_pci_driver);
    if (g_day25_class)
        class_destroy(g_day25_class);
    unregister_chrdev_region(g_day25_base_dev, DAY25_MAX_MINORS);
    pr_info("day25_edu_irq: module unloaded");
}

module_init(day25_init);
module_exit(day25_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day25 EDU MSI interrupt experiment");
MODULE_LICENSE("GPL");
