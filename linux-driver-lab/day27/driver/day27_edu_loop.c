// SPDX-License-Identifier: GPL-2.0
/*
 * Day27 - remove/unload symmetry + 200-loop stress
 *
 * 本日目标不是继续扩功能，而是把“重复 insmod/rmmod 是否稳定”做扎实。
 * 因此驱动设计尽量简单、可重复、可观测：
 * 1. 继续基于 Day25/Day26 已经验证过的 QEMU EDU 设备；
 * 2. 保留最小用户态可观测接口（read / write / ioctl）；
 * 3. 在 remove() 中严格对称释放：
 *    - device_destroy
 *    - cdev_del
 *    - free_irq / pci_free_irq_vectors
 *    - pci_iounmap
 *    - pci_release_regions
 *    - pci_disable_device
 *    - kfree
 * 4. 每轮循环只做一次最小 smoke：打开 /dev、触发一次 IRQ、确认 count>0，再卸载。
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

#include "include/day27_edu_loop.h"
#include "../include/day27_edu_uapi.h"

static dev_t g_day27_base_dev;
static struct class *g_day27_class;
static atomic_t g_day27_minor = ATOMIC_INIT(0);

/*
 * 统一封装 BAR0 寄存器读写。Day27 没有复杂寄存器协议，只需要：
 * - 向 EDU 的 IRQ_RAISE 寄存器写入一个非 0 值来触发中断；
 * - 在 IRQ handler 中读取 IRQ_STATUS 并写 IRQ_ACK 完成清中断。
 */
static inline u32 day27_read32(struct day27_dev *d, u32 off)
{
    return readl(d->bar0 + off);
}

static inline void day27_write32(struct day27_dev *d, u32 off, u32 val)
{
    writel(val, d->bar0 + off);
}

/*
 * Day27 的中断处理函数非常克制：
 * 1. 先读 IRQ_STATUS；
 * 2. 记录本轮看到的 status 和累计次数；
 * 3. 把相同值写回 IRQ_ACK；
 * 4. 打日志，供 records 统计。
 *
 * 这样做的目的不是追求性能，而是为了在 200 次循环里最大化可观测性。
 */
static irqreturn_t day27_irq_handler(int irq, void *opaque)
{
    struct day27_dev *d = opaque;
    unsigned long flags;
    u32 status;

    status = day27_read32(d, DAY27_EDU_REG_IRQ_STATUS);
    if (!status)
        return IRQ_NONE;

    spin_lock_irqsave(&d->irq_lock, flags);
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);

    /* EDU 需要把相同状态值写回 IRQ_ACK 以完成 ACK。 */
    day27_write32(d, DAY27_EDU_REG_IRQ_ACK, status);

    dev_info(&d->pdev->dev,
             "irq handler: irq=%d status=0x%08x count=%llu\n",
             irq, status, d->irq_count);
    return IRQ_HANDLED;
}

/*
 * 把当前设备状态拼成一段文本。
 * Day27 的 read() 接口不追求结构化，而是为了 guest 脚本和 records 直观看到：
 * - 当前设备是谁；
 * - BAR0 资源范围；
 * - IRQ vector / irq_count；
 * - 最近一次中断的 status / ack。
 */
static ssize_t day27_build_state_text(struct day27_dev *d, char *buf, size_t size)
{
    return scnprintf(buf, size,
                     "vendor=0x%04x device=0x%04x\n"
                     "bar0_start=0x%llx bar0_len=0x%llx\n"
                     "irq_vector=%u irq_count=%llu msi_enabled=%u\n"
                     "last_irq_status=0x%08x last_ack_value=0x%08x\n",
                     d->pdev->vendor,
                     d->pdev->device,
                     (unsigned long long)d->bar0_start,
                     (unsigned long long)d->bar0_len,
                     d->irq_vector,
                     d->irq_count,
                     !!(d->pdev->msi_enabled),
                     d->last_irq_status,
                     d->last_ack_value);
}

/*
 * open 只做最小工作：把 day27_dev 塞进 private_data，
 * 后续 read/write/ioctl 统一从 file->private_data 取设备上下文。
 */
static int day27_open(struct inode *inode, struct file *file)
{
    struct day27_dev *d = container_of(inode->i_cdev, struct day27_dev, cdev);
    file->private_data = d;
    return 0;
}

static ssize_t day27_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct day27_dev *d = file->private_data;
    char kbuf[192];
    ssize_t len;

    if (!d || !d->bar0)
        return -ENODEV;

    len = day27_build_state_text(d, kbuf, sizeof(kbuf));
    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

/*
 * write() 的语义是：用户态写一个十进制/十六进制整数，驱动把它写到 EDU 的 IRQ_RAISE 寄存器。
 * 约束：
 * - 不能为空；
 * - 不能太长；
 * - 必须能被解析成整数；
 * - 0 不允许，因为 Day27 约定“非 0 才触发中断”。
 */
static ssize_t day27_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    struct day27_dev *d = file->private_data;
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
    day27_write32(d, DAY27_EDU_REG_IRQ_RAISE, (u32)v);
    *ppos += count;
    return count;
}

/*
 * ioctl 提供结构化接口，给用户态工具做更稳定的校验。
 * Day27 保留最小集合：
 * - GET_INFO：看设备/中断/最近状态；
 * - GET_IRQ_COUNT：看累计中断次数；
 * - RESET_STATS：把计数归零，方便每轮循环都从干净状态开始。
 */
static long day27_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day27_dev *d = file->private_data;

    switch (cmd) {
    case DAY27_IOC_GET_INFO: {
        struct day27_info info = {
            .tool_api_version = DAY27_TOOL_API_VERSION,
            .vendor_id = d->pdev->vendor,
            .device_id = d->pdev->device,
            .irq_vector = d->irq_vector,
            .irq_count = d->irq_count,
            .last_irq_status = d->last_irq_status,
            .last_ack_value = d->last_ack_value,
            .bar0_start = d->bar0_start,
            .bar0_len = d->bar0_len,
            .msi_enabled = !!(d->pdev->msi_enabled),
        };
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }
    case DAY27_IOC_GET_IRQ_COUNT: {
        struct day27_irq_count cnt = { .count = d->irq_count };
        if (copy_to_user((void __user *)arg, &cnt, sizeof(cnt)))
            return -EFAULT;
        return 0;
    }
    case DAY27_IOC_RESET_STATS:
        d->irq_count = 0;
        d->last_irq_status = 0;
        d->last_ack_value = 0;
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations day27_fops = {
    .owner = THIS_MODULE,
    .open = day27_open,
    .read = day27_read,
    .write = day27_write,
    .unlocked_ioctl = day27_ioctl,
    .llseek = default_llseek,
};

/*
 * 创建字符设备节点 /dev/day27_eduX。
 * 每次 probe 分配一个 minor，remove 时成对销毁。
 * Day27 循环稳定性的一个关键点就是：这里分配出去的对象必须在 remove 中完整回收。
 */
static int day27_setup_chrdev(struct day27_dev *d)
{
    int ret;
    int minor = atomic_fetch_add(1, &g_day27_minor);

    d->devt = MKDEV(MAJOR(g_day27_base_dev), minor);
    cdev_init(&d->cdev, &day27_fops);
    d->cdev.owner = THIS_MODULE;

    ret = cdev_add(&d->cdev, d->devt, 1);
    if (ret)
        return ret;

    d->device = device_create(g_day27_class, &d->pdev->dev, d->devt, d,
                              DAY27_DEV_NAME_FMT, minor);
    if (IS_ERR(d->device)) {
        ret = PTR_ERR(d->device);
        d->device = NULL;
        cdev_del(&d->cdev);
        return ret;
    }
    return 0;
}

/*
 * 与 day27_setup_chrdev() 对称的释放路径。
 */
static void day27_destroy_chrdev(struct day27_dev *d)
{
    if (d->device)
        device_destroy(g_day27_class, d->devt);
    cdev_del(&d->cdev);
}

/*
 * probe 路径：
 * 1. 分配软件对象；
 * 2. enable / request_regions / set_master；
 * 3. ioremap BAR0；
 * 4. 申请 MSI（失败时回退到 LEGACY）；
 * 5. request_irq；
 * 6. 创建字符设备；
 * 7. 打出足够多的日志，供 Day27 records 统计。
 */
static int day27_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day27_dev *d;
    int ret;

    dev_info(&pdev->dev, "probe enter: %04x:%04x\n", pdev->vendor, pdev->device);

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->pdev = pdev;
    pci_set_drvdata(pdev, d);
    spin_lock_init(&d->irq_lock);

    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
        goto err_free;
    }

    ret = pci_request_regions(pdev, DAY27_DRV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", ret);
        goto err_disable;
    }

    pci_set_master(pdev);

    d->bar0_start = pci_resource_start(pdev, 0);
    d->bar0_len   = pci_resource_len(pdev, 0);
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

    /*
     * Day27 的重点是稳定性，所以这里优先申请 MSI，必要时允许回退到传统 LEGACY。
     */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
    if (ret < 0) {
        dev_err(&pdev->dev, "pci_alloc_irq_vectors failed: %d\n", ret);
        goto err_iounmap;
    }
    d->irq_vector = pci_irq_vector(pdev, 0);

    ret = request_irq(d->irq_vector, day27_irq_handler, 0, DAY27_DRV_NAME, d);
    if (ret) {
        dev_err(&pdev->dev, "request_irq(%u) failed: %d\n", d->irq_vector, ret);
        goto err_irq_vectors;
    }
    dev_info(&pdev->dev, "MSI vector=%u enabled=%u\n",
             d->irq_vector, !!pdev->msi_enabled);

    ret = day27_setup_chrdev(d);
    if (ret) {
        dev_err(&pdev->dev, "day27_setup_chrdev failed: %d\n", ret);
        goto err_irq;
    }

    dev_info(&pdev->dev, "probe success\n");
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
 * remove 路径必须与 probe 严格对称。
 * Day27 的 200 次循环能否通过，本质上就看这里会不会漏资源、会不会留下脏状态。
 */
static void day27_remove(struct pci_dev *pdev)
{
    struct day27_dev *d = pci_get_drvdata(pdev);

    if (!d)
        return;

    dev_info(&pdev->dev, "remove enter\n");
    day27_destroy_chrdev(d);
    free_irq(d->irq_vector, d);
    pci_free_irq_vectors(pdev);
    if (d->bar0)
        pci_iounmap(pdev, d->bar0);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    kfree(d);
    dev_info(&pdev->dev, "remove leave\n");
}

static const struct pci_device_id day27_pci_ids[] = {
    { PCI_DEVICE(DAY27_EDU_VENDOR_ID, DAY27_EDU_DEVICE_ID) },
    { }
};
MODULE_DEVICE_TABLE(pci, day27_pci_ids);

static struct pci_driver day27_pci_driver = {
    .name = DAY27_DRV_NAME,
    .id_table = day27_pci_ids,
    .probe = day27_probe,
    .remove = day27_remove,
};

static int __init day27_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&g_day27_base_dev, 0, 32, DAY27_DRV_NAME);
    if (ret)
        return ret;

    g_day27_class = class_create(THIS_MODULE, DAY27_CLASS_NAME);
    if (IS_ERR(g_day27_class)) {
        ret = PTR_ERR(g_day27_class);
        unregister_chrdev_region(g_day27_base_dev, 32);
        return ret;
    }

    ret = pci_register_driver(&day27_pci_driver);
    if (ret) {
        class_destroy(g_day27_class);
        unregister_chrdev_region(g_day27_base_dev, 32);
        return ret;
    }

    pr_info("%s: loaded\n", DAY27_DRV_NAME);
    return 0;
}

static void __exit day27_exit(void)
{
    pci_unregister_driver(&day27_pci_driver);
    class_destroy(g_day27_class);
    unregister_chrdev_region(g_day27_base_dev, 32);
    pr_info("%s: unloaded\n", DAY27_DRV_NAME);
}

module_init(day27_init);
module_exit(day27_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Day27 EDU loop/remove symmetry driver");
