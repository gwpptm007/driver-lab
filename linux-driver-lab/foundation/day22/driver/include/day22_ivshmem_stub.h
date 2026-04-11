/* SPDX-License-Identifier: GPL-2.0 */
/*
 * day22_ivshmem_stub.h
 *
 * 这不是已经完成的 pci_driver，而是 day22 提前放好的“最小骨架头文件”。
 * 目的：
 *   1. 让 day22 就开始出现真正的内核 C 代码，而不是只剩脚本和文档。
 *   2. 把 day23 要写的 probe/remove、BAR、IRQ 相关状态先梳理出来。
 *   3. 让你读代码时知道：真正的 pci_driver 需要维护哪些成员。
 */

#ifndef DAY22_IVSHMEM_STUB_H
#define DAY22_IVSHMEM_STUB_H

#include <linux/pci.h>
#include <linux/types.h>

#define DAY22_IVSHMEM_VENDOR_ID 0x1af4
#define DAY22_IVSHMEM_DEVICE_ID 0x1110

/*
 * day22_stub_dev:
 *   day23 真正进入 probe/remove 后，最核心的私有数据大概率就是这些成员。
 *
 * pdev         : PCI 核心传进来的设备对象。
 * bar0         : 后面会通过 pci_iomap() 映射出来的 BAR0 基址。
 * bar0_len     : BAR0 资源长度，调试 resource 尤其有用。
 * irq_vector   : 预留给 day25 的 MSI / 中断实验。
 * irq_count    : 中断统计计数，后面会通过 procfs/debugfs/ioctl 暴露给用户态。
 */
struct day22_stub_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_len;
    int irq_vector;
    u64 irq_count;
};

#endif /* DAY22_IVSHMEM_STUB_H */
