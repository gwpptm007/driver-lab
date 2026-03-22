// SPDX-License-Identifier: GPL-2.0
/*
 * day24_ivshmem_mmio.c
 *
 * day24 的目标：
 *   1. 在 day23 probe/remove + BAR 映射成功的基础上继续前进
 *   2. 不去乱写 ivshmem BAR0 寄存器
 *   3. 把 BAR2 当成共享内存窗口，在其起始位置定义最小协议头
 *   4. 协议头字段通过 readl/writel 访问
 *   5. payload 通过 memcpy_toio/fromio 访问
 *   6. 通过一个最小 misc char device 暴露给用户态验证
 *
 * 注意：day24 的“MMIO 读写”主要指对 BAR2 协议头字段做 readl/writel，
 * 这是我们自己控制的共享内存头，不是随意写 BAR0 硬件寄存器。
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "include/day24_ivshmem_mmio.h"

static const struct pci_device_id day24_pci_ids[] = {
    { PCI_DEVICE(DAY24_IVSHMEM_VENDOR_ID, DAY24_IVSHMEM_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, day24_pci_ids);

static bool day24_bar_should_map(struct pci_dev *pdev, int bar)
{
    resource_size_t len = pci_resource_len(pdev, bar);
    unsigned long flags = pci_resource_flags(pdev, bar);

    if (!len)
        return false;
    if (!(flags & IORESOURCE_MEM))
        return false;
    return true;
}

static void day24_dump_bar(struct pci_dev *pdev, struct day24_dev *d, int bar)
{
    struct day24_bar_info *bi = &d->bar[bar];

    bi->index = bar;
    bi->start = pci_resource_start(pdev, bar);
    bi->end = pci_resource_end(pdev, bar);
    bi->len = pci_resource_len(pdev, bar);
    bi->flags = pci_resource_flags(pdev, bar);

    dev_info(&pdev->dev,
             "BAR%d: start=%pa end=%pa len=%pa flags=0x%lx\n",
             bar, &bi->start, &bi->end, &bi->len, bi->flags);
}

static void day24_unmap_bars(struct day24_dev *d)
{
    int i;

    for (i = 0; i < PCI_STD_NUM_BARS; i++) {
        if (!d->bar[i].vaddr)
            continue;
        pci_iounmap(d->pdev, d->bar[i].vaddr);
        d->bar[i].vaddr = NULL;
    }
}

static int day24_map_bar(struct day24_dev *d, int bar)
{
    void __iomem *vaddr;

    if (!day24_bar_should_map(d->pdev, bar))
        return 0;

    vaddr = pci_iomap(d->pdev, bar, 0);
    if (!vaddr) {
        dev_err(&d->pdev->dev, "BAR%d: pci_iomap failed\n", bar);
        return -ENOMEM;
    }

    d->bar[bar].vaddr = vaddr;
    dev_info(&d->pdev->dev, "BAR%d mapped: vaddr=%p\n", bar, vaddr);
    return 0;
}

static inline void __iomem *day24_bar2_ptr(struct day24_dev *d, u32 off)
{
    return d->bar[2].vaddr + off;
}

static u32 day24_proto_read32(struct day24_dev *d, u32 off)
{
    return readl(day24_bar2_ptr(d, off));
}

static void day24_proto_write32(struct day24_dev *d, u32 off, u32 val)
{
    writel(val, day24_bar2_ptr(d, off));
}

static size_t day24_payload_capacity(struct day24_dev *d)
{
    if (d->bar[2].len <= DAY24_PROTO_PAYLOAD_OFF)
        return 0;
    return min_t(size_t,
                 (size_t)(d->bar[2].len - DAY24_PROTO_PAYLOAD_OFF),
                 DAY24_PROTO_MAX_PAYLOAD);
}

static void day24_proto_init_if_needed(struct day24_dev *d)
{
    u32 magic;
    u32 version;
    size_t cap = day24_payload_capacity(d);

    if (!d->bar[2].vaddr)
        return;

    magic = day24_proto_read32(d, DAY24_PROTO_OFF_MAGIC);
    version = day24_proto_read32(d, DAY24_PROTO_OFF_VERSION);

    if (magic == DAY24_PROTO_MAGIC && version == DAY24_PROTO_VERSION) {
        dev_info(&d->pdev->dev,
                 "protocol header exists: magic=0x%08x version=%u seq=%u state=%u len=%u\n",
                 magic,
                 version,
                 day24_proto_read32(d, DAY24_PROTO_OFF_SEQ),
                 day24_proto_read32(d, DAY24_PROTO_OFF_STATE),
                 day24_proto_read32(d, DAY24_PROTO_OFF_LEN));
        return;
    }

    day24_proto_write32(d, DAY24_PROTO_OFF_MAGIC, DAY24_PROTO_MAGIC);
    day24_proto_write32(d, DAY24_PROTO_OFF_VERSION, DAY24_PROTO_VERSION);
    day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, 0);
    day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_READY);
    day24_proto_write32(d, DAY24_PROTO_OFF_LEN, 0);

    if (cap)
        memset_io(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF), 0, cap);

    dev_info(&d->pdev->dev,
             "protocol header initialized: magic=0x%08x version=%u payload_cap=%zu\n",
             DAY24_PROTO_MAGIC, DAY24_PROTO_VERSION, cap);
}

static bool day24_mmio_offset_allowed(u32 off)
{
    switch (off) {
    case DAY24_PROTO_OFF_SEQ:
    case DAY24_PROTO_OFF_STATE:
    case DAY24_PROTO_OFF_LEN:
        return true;
    default:
        return false;
    }
}

static int day24_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct day24_dev *d = container_of(misc, struct day24_dev, miscdev);

    file->private_data = d;
    return 0;
}

static loff_t day24_llseek(struct file *file, loff_t off, int whence)
{
    struct day24_dev *d = file->private_data;
    loff_t newpos;
    loff_t limit = (loff_t)day24_payload_capacity(d);

    switch (whence) {
    case SEEK_SET:
        newpos = off;
        break;
    case SEEK_CUR:
        newpos = file->f_pos + off;
        break;
    case SEEK_END:
        newpos = limit + off;
        break;
    default:
        return -EINVAL;
    }

    if (newpos < 0 || newpos > limit)
        return -EINVAL;

    file->f_pos = newpos;
    return newpos;
}

static ssize_t day24_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct day24_dev *d = file->private_data;
    size_t payload_len;
    size_t cap;
    size_t to_copy;
    void *kbuf;
    int rc = 0;

    if (!d->bar[2].vaddr)
        return -ENODEV;

    mutex_lock(&d->lock);

    cap = day24_payload_capacity(d);
    payload_len = day24_proto_read32(d, DAY24_PROTO_OFF_LEN);
    if (payload_len > cap)
        payload_len = cap;

    if (*ppos >= payload_len)
        goto out_zero;

    to_copy = min_t(size_t, count, payload_len - (size_t)*ppos);
    kbuf = kmalloc(to_copy, GFP_KERNEL);
    if (!kbuf) {
        rc = -ENOMEM;
        goto out_unlock;
    }

    memcpy_fromio(kbuf,
                  day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF + (u32)*ppos),
                  to_copy);
    if (copy_to_user(buf, kbuf, to_copy)) {
        kfree(kbuf);
        rc = -EFAULT;
        goto out_unlock;
    }

    kfree(kbuf);
    *ppos += to_copy;
    rc = (int)to_copy;
    goto out_unlock;

out_zero:
    rc = 0;
out_unlock:
    mutex_unlock(&d->lock);
    return rc;
}

static ssize_t day24_write(struct file *file, const char __user *buf, size_t count,
                           loff_t *ppos)
{
    struct day24_dev *d = file->private_data;
    size_t cap;
    size_t to_copy;
    size_t new_len;
    void *kbuf;
    u32 seq;
    int rc = 0;

    if (!d->bar[2].vaddr)
        return -ENODEV;

    mutex_lock(&d->lock);

    cap = day24_payload_capacity(d);
    if (*ppos >= cap) {
        rc = -ENOSPC;
        goto out_unlock;
    }

    to_copy = min_t(size_t, count, cap - (size_t)*ppos);
    kbuf = memdup_user(buf, to_copy);
    if (IS_ERR(kbuf)) {
        rc = PTR_ERR(kbuf);
        goto out_unlock;
    }

    memcpy_toio(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF + (u32)*ppos),
                kbuf, to_copy);
    kfree(kbuf);

    *ppos += to_copy;
    new_len = max_t(size_t, day24_proto_read32(d, DAY24_PROTO_OFF_LEN), (size_t)*ppos);
    day24_proto_write32(d, DAY24_PROTO_OFF_LEN, (u32)new_len);
    day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_USER_WRITTEN);
    seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ) + 1;
    day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq);

    dev_info(&d->pdev->dev,
             "payload write: count=%zu new_len=%zu seq=%u\n",
             to_copy, new_len, seq);

    rc = (int)to_copy;
out_unlock:
    mutex_unlock(&d->lock);
    return rc;
}

static long day24_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct day24_dev *d = file->private_data;
    struct day24_info_uapi info;
    struct day24_mmio32_uapi mmio;
    u32 seq;
    long rc = 0;

    if (!d->bar[2].vaddr)
        return -ENODEV;

    mutex_lock(&d->lock);

    switch (cmd) {
    case DAY24_IOC_GET_INFO:
        memset(&info, 0, sizeof(info));
        info.vendor = d->pdev->vendor;
        info.device = d->pdev->device;
        info.bar0_first_dword = d->bar0_first_dword;
        info.proto_magic = day24_proto_read32(d, DAY24_PROTO_OFF_MAGIC);
        info.proto_version = day24_proto_read32(d, DAY24_PROTO_OFF_VERSION);
        info.proto_seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ);
        info.proto_state = day24_proto_read32(d, DAY24_PROTO_OFF_STATE);
        info.proto_payload_len = day24_proto_read32(d, DAY24_PROTO_OFF_LEN);

        info.bar0.index = 0;
        info.bar0.start = d->bar[0].start;
        info.bar0.end = d->bar[0].end;
        info.bar0.len = d->bar[0].len;
        info.bar0.flags = d->bar[0].flags;

        info.bar2.index = 2;
        info.bar2.start = d->bar[2].start;
        info.bar2.end = d->bar[2].end;
        info.bar2.len = d->bar[2].len;
        info.bar2.flags = d->bar[2].flags;

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            rc = -EFAULT;
        break;

    case DAY24_IOC_MMIO_READ32:
        if (copy_from_user(&mmio, (void __user *)arg, sizeof(mmio))) {
            rc = -EFAULT;
            break;
        }
        if ((mmio.offset & 0x3) || mmio.offset >= DAY24_PROTO_PAYLOAD_OFF ||
            mmio.offset + sizeof(u32) > d->bar[2].len) {
            rc = -EINVAL;
            break;
        }
        mmio.value = day24_proto_read32(d, mmio.offset);
        if (copy_to_user((void __user *)arg, &mmio, sizeof(mmio)))
            rc = -EFAULT;
        break;

    case DAY24_IOC_MMIO_WRITE32:
        if (copy_from_user(&mmio, (void __user *)arg, sizeof(mmio))) {
            rc = -EFAULT;
            break;
        }
        if (!day24_mmio_offset_allowed(mmio.offset) ||
            mmio.offset + sizeof(u32) > d->bar[2].len) {
            rc = -EINVAL;
            break;
        }
        day24_proto_write32(d, mmio.offset, mmio.value);
        seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ) + 1;
        day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq);
        dev_info(&d->pdev->dev,
                 "MMIO write: offset=0x%08x value=0x%08x seq=%u\n",
                 mmio.offset, mmio.value, seq);
        break;

    case DAY24_IOC_CLEAR_PAYLOAD:
        memset_io(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF), 0,
                  day24_payload_capacity(d));
        day24_proto_write32(d, DAY24_PROTO_OFF_LEN, 0);
        day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_EMPTY);
        seq = day24_proto_read32(d, DAY24_PROTO_OFF_SEQ) + 1;
        day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq);
        dev_info(&d->pdev->dev, "payload cleared: seq=%u\n", seq);
        break;

    default:
        rc = -ENOTTY;
        break;
    }

    mutex_unlock(&d->lock);
    return rc;
}

static const struct file_operations day24_fops = {
    .owner = THIS_MODULE,
    .open = day24_open,
    .llseek = day24_llseek,
    .read = day24_read,
    .write = day24_write,
    .unlocked_ioctl = day24_ioctl,
};

static int day24_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day24_dev *d;
    int rc;

    dev_info(&pdev->dev,
             "probe enter: vendor=%04x device=%04x class=0x%06x irq=%u\n",
             pdev->vendor, pdev->device, pdev->class, pdev->irq);

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    d->pdev = pdev;
    mutex_init(&d->lock);
    pci_set_drvdata(pdev, d);

    rc = pci_enable_device(pdev);
    if (rc) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", rc);
        goto err_free;
    }
    d->device_enabled = true;

    rc = pci_request_regions(pdev, DAY24_DRV_NAME);
    if (rc) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", rc);
        goto err_disable;
    }
    d->regions_claimed = true;

    pci_set_master(pdev);

    day24_dump_bar(pdev, d, 0);
    day24_dump_bar(pdev, d, 2);

    rc = day24_map_bar(d, 0);
    if (rc)
        goto err_regions;

    rc = day24_map_bar(d, 2);
    if (rc)
        goto err_unmap;

    if (d->bar[0].vaddr) {
        d->bar0_first_dword = readl(d->bar[0].vaddr);
        dev_info(&pdev->dev, "BAR0 first dword=0x%08x\n", d->bar0_first_dword);
    }

    day24_proto_init_if_needed(d);

    d->miscdev.minor = MISC_DYNAMIC_MINOR;
    d->miscdev.name = DAY24_DEVICE_NAME;
    d->miscdev.fops = &day24_fops;
    d->miscdev.parent = &pdev->dev;
    rc = misc_register(&d->miscdev);
    if (rc) {
        dev_err(&pdev->dev, "misc_register failed: %d\n", rc);
        goto err_unmap;
    }

    dev_info(&pdev->dev,
             "probe success: device=%s payload_cap=%zu\n",
             DAY24_DEVICE_NAME, day24_payload_capacity(d));
    return 0;

err_unmap:
    day24_unmap_bars(d);
err_regions:
    if (d->regions_claimed) {
        pci_release_regions(pdev);
        d->regions_claimed = false;
    }
err_disable:
    if (d->device_enabled) {
        pci_disable_device(pdev);
        d->device_enabled = false;
    }
err_free:
    pci_set_drvdata(pdev, NULL);
    kfree(d);
    return rc;
}

static void day24_remove(struct pci_dev *pdev)
{
    struct day24_dev *d = pci_get_drvdata(pdev);

    dev_info(&pdev->dev, "remove enter\n");

    if (!d)
        return;

    misc_deregister(&d->miscdev);
    day24_unmap_bars(d);

    if (d->regions_claimed)
        pci_release_regions(pdev);

    if (d->device_enabled)
        pci_disable_device(pdev);

    pci_set_drvdata(pdev, NULL);
    kfree(d);

    dev_info(&pdev->dev, "remove leave\n");
}

static struct pci_driver day24_pci_driver = {
    .name = DAY24_DRV_NAME,
    .id_table = day24_pci_ids,
    .probe = day24_probe,
    .remove = day24_remove,
};

module_pci_driver(day24_pci_driver);

MODULE_AUTHOR("OpenAI / WangQi day24 lab");
MODULE_DESCRIPTION("day24 ivshmem MMIO lab: BAR2 protocol + misc device + user tool");
MODULE_LICENSE("GPL");
