// SPDX-License-Identifier: GPL-2.0
/*
 * day22_ivshmem_stub.c
 *
 * 这是 day22 提前准备的最小 pci_driver 骨架，目的不是在今天就完成 BAR/MMIO/MSI，
 * 而是给 day23 一个可以直接接着写的 C 代码起点。
 *
 * 今天保留“能编译、能匹配、先不接管复杂逻辑”的风格：
 *   - 有 pci_device_id
 *   - 有 pci_driver
 *   - 有 probe/remove
 *   - probe 里只做资源信息打印，不做 enable_device/request_regions/pci_iomap
 *
 * 为什么 probe 里故意不把完整逻辑写完：
 *   day22 的任务仍然是“先枚举看见设备”；
 *   day23 的任务才是“pci_enable_device/request_regions/pci_iomap”。
 *
 * 这样拆，学习时更容易看清阶段边界：
 *   day22 = 设备发现 + C 骨架
 *   day23 = 资源接管
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include "include/day22_ivshmem_stub.h"

#define DRV_NAME "day22_ivshmem_stub"

static const struct pci_device_id day22_pci_ids[] = {
    { PCI_DEVICE(DAY22_IVSHMEM_VENDOR_ID, DAY22_IVSHMEM_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, day22_pci_ids);

static void day22_dump_resources(struct pci_dev *pdev)
{
    int bar;

    for (bar = 0; bar < PCI_STD_NUM_BARS; bar++) {
        resource_size_t start = pci_resource_start(pdev, bar);
        resource_size_t end = pci_resource_end(pdev, bar);
        resource_size_t len = pci_resource_len(pdev, bar);
        unsigned long flags = pci_resource_flags(pdev, bar);

        if (!len)
            continue;

        dev_info(&pdev->dev,
                 "BAR%d: start=%pa end=%pa len=%pa flags=0x%lx\n",
                 bar, &start, &end, &len, flags);
    }
}

static int day22_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct day22_stub_dev *sdev;

    dev_info(&pdev->dev,
             "probe enter: vendor=%04x device=%04x class=0x%06x irq=%u\n",
             pdev->vendor, pdev->device, pdev->class, pdev->irq);

    /*
     * 这里先只申请一个零碎的私有结构体，证明 probe/remove 的生命周期可以闭合。
     * 后面 day23 会在这个结构体里继续填 BAR、IRQ 等运行时状态。
     */
    sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
    if (!sdev)
        return -ENOMEM;

    sdev->pdev = pdev;
    sdev->irq_vector = -1;
    pci_set_drvdata(pdev, sdev);

    day22_dump_resources(pdev);

    /*
     * 故意不在 day22 做下面这些动作：
     *   pci_enable_device()
     *   pci_request_regions()
     *   pci_iomap()
     *   pci_alloc_irq_vectors()
     *
     * 它们会在 day23/day25 逐步补上。
     */
    dev_info(&pdev->dev,
             "day22 stub matched successfully; real BAR/MSI work starts on day23/day25\n");
    return 0;
}

static void day22_remove(struct pci_dev *pdev)
{
    struct day22_stub_dev *sdev = pci_get_drvdata(pdev);

    dev_info(&pdev->dev,
             "remove enter: sdev=%p irq_vector=%d irq_count=%llu\n",
             sdev, sdev ? sdev->irq_vector : -1,
             sdev ? sdev->irq_count : 0ULL);
}

static struct pci_driver day22_pci_driver = {
    .name = DRV_NAME,
    .id_table = day22_pci_ids,
    .probe = day22_probe,
    .remove = day22_remove,
};

module_pci_driver(day22_pci_driver);

MODULE_AUTHOR("OpenAI / WangQi day22 lab scaffold");
MODULE_DESCRIPTION("day22 ivshmem pci stub for learning probe/remove lifecycle");
MODULE_LICENSE("GPL");
