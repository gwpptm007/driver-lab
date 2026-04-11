// SPDX-License-Identifier: GPL-2.0
/*
 * day23_ivshmem_probe.c
 *
 * day23 的目标不是做完整共享内存协议，而是先把一个“最小可运行”的 pci_driver
 * 做出来，让它在 guest 里完成：
 *   1. 匹配 ivshmem 设备
 *   2. pci_enable_device()
 *   3. pci_request_regions()
 *   4. pci_set_master()
 *   5. pci_iomap() 把 BAR0 / BAR2 映射出来
 *   6. 在 remove() 中对称释放
 *
 * 代码刻意保留大量注释，方便后续 day24 在此基础上继续加 MMIO 读写。
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include "include/day23_ivshmem_probe.h"

static const struct pci_device_id day23_pci_ids[] = {
    { PCI_DEVICE(DAY23_IVSHMEM_VENDOR_ID, DAY23_IVSHMEM_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, day23_pci_ids);

static bool day23_bar_should_map(struct pci_dev *pdev, int bar)
{
    resource_size_t len = pci_resource_len(pdev, bar);
    unsigned long flags = pci_resource_flags(pdev, bar);

    if (!len)
        return false;
    if (!(flags & IORESOURCE_MEM))
        return false;
    return true;
}

static void day23_dump_bar(struct pci_dev *pdev, struct day23_dev *d, int bar)
{
    struct day23_bar_info *bi = &d->bar[bar];

    bi->index = bar;
    bi->start = pci_resource_start(pdev, bar);
    bi->end = pci_resource_end(pdev, bar);
    bi->len = pci_resource_len(pdev, bar);
    bi->flags = pci_resource_flags(pdev, bar);

    dev_info(&pdev->dev,
             "BAR%d: start=%pa end=%pa len=%pa flags=0x%lx\n",
             bar, &bi->start, &bi->end, &bi->len, bi->flags);
}

static void day23_unmap_bars(struct day23_dev *d)
{
    int i;

    for (i = 0; i < PCI_STD_NUM_BARS; i++) {
        if (!d->bar[i].vaddr)
            continue;

        pci_iounmap(d->pdev, d->bar[i].vaddr);
        d->bar[i].vaddr = NULL;
    }
}

static int day23_map_bar(struct day23_dev *d, int bar)
{
    void __iomem *vaddr;

    if (!day23_bar_should_map(d->pdev, bar))
        return 0;

    // 内核态：调用 pci_iomap() 或 ioremap()，将物理地址转为内核虚拟地址
    vaddr = pci_iomap(d->pdev, bar, 0);
    if (!vaddr) {
        dev_err(&d->pdev->dev, "BAR%d: pci_iomap failed\n", bar);
        return -ENOMEM;
    }

    d->bar[bar].vaddr = vaddr;
    dev_info(&d->pdev->dev, "BAR%d mapped: vaddr=%p\n", bar, vaddr);
    return 0;
}

static int day23_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day23_dev *d;
    int rc;

    dev_info(&pdev->dev,
             "probe enter: vendor=%04x device=%04x class=0x%06x irq=%u\n",
             pdev->vendor, pdev->device, pdev->class, pdev->irq);

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    d->pdev = pdev;
    pci_set_drvdata(pdev, d);

    rc = pci_enable_device(pdev);
    if (rc) {
        dev_err(&pdev->dev, "pci_enable_device failed: %d\n", rc);
        goto err_free;
    }
    d->device_enabled = true;

    rc = pci_request_regions(pdev, DAY23_DRV_NAME);
    if (rc) {
        dev_err(&pdev->dev, "pci_request_regions failed: %d\n", rc);
        goto err_disable;
    }
    d->regions_claimed = true;

    pci_set_master(pdev);

    day23_dump_bar(pdev, d, 0);
    day23_dump_bar(pdev, d, 2);

    rc = day23_map_bar(d, 0);
    if (rc)
        goto err_regions;

    rc = day23_map_bar(d, 2);
    if (rc)
        goto err_unmap;

    if (d->bar[0].vaddr) {
        /*
         * ivshmem BAR0 是寄存器窗口。day23 只读第一个 dword，
         * 目的是证明 MMIO 映射已经真实可访问；真正协议读写留到 day24。
         */
        d->bar0_first_dword = readl(d->bar[0].vaddr);
        dev_info(&pdev->dev, "BAR0 first dword=0x%08x\n", d->bar0_first_dword);
    }

    dev_info(&pdev->dev, "probe success\n");
    return 0;

err_unmap:
    day23_unmap_bars(d);
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

static void day23_remove(struct pci_dev *pdev)
{
    struct day23_dev *d = pci_get_drvdata(pdev);

    dev_info(&pdev->dev, "remove enter\n");

    if (!d)
        return;

    day23_unmap_bars(d);

    if (d->regions_claimed)
        pci_release_regions(pdev);

    if (d->device_enabled)
        pci_disable_device(pdev);

    pci_set_drvdata(pdev, NULL);
    kfree(d);

    dev_info(&pdev->dev, "remove leave\n");
}

static struct pci_driver day23_pci_driver = {
    .name = DAY23_DRV_NAME,
    .id_table = day23_pci_ids,
    .probe = day23_probe,
    .remove = day23_remove,
};

module_pci_driver(day23_pci_driver);

MODULE_AUTHOR("OpenAI / WangQi day23 lab");
MODULE_DESCRIPTION("day23 ivshmem pci probe lab: enable/request/iomap/remove");
MODULE_LICENSE("GPL");
