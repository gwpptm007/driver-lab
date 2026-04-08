// SPDX-License-Identifier: GPL-2.0
/*
 * day22_ivshmem_stub.c - Linux PCI 驱动骨架
 *
 * ==================== 代码框架总览 ====================
 *
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │                    pci_driver 骨架                          │
 *  │                                                             │
 *  │  1. pci_device_id 数组  ──► 告诉内核"我匹配哪些设备"         │
 *  │  2. pci_driver 结构体  ──► 注册到内核的核心结构               │
 *  │  3. probe / remove   ──► 设备插入/拔出时的回调                │
 *  │  4. module_pci_driver() ──► 模块入口（简化写法）             │
 *  └─────────────────────────────────────────────────────────────┘
 *
 * ==================== 学习重点 ====================
 *
 *  day22 的 stub 驱动是"故意简化"的骨架：
 *  - probe 里只做打印，不做 pci_enable_device / request_regions / pci_iomap
 *  - 目的是让你看清 pci_driver 的最小结构
 *  - 真实资源操作在 day23/day25 逐步补上
 *
 *  阶段分工：
 *    day22 = 设备发现 + C 骨架
 *    day23 = 资源接管（pci_enable / request_regions / iomap）
 *    day25 = MSI 中断（pci_alloc_irq_vectors / request_irq）
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include "include/day22_ivshmem_stub.h"

/*
 * ==================== 第1步：定义驱动名称 ====================
 */
#define DRV_NAME "day22_ivshmem_stub"

/*
 * ==================== 第2步：定义支持的设备列表 ====================
 *
 * pci_device_id 数组告诉内核：
 *   "当枚举到哪个 vendor:device 时，调用我这个驱动的 probe"
 *
 * 格式：PCI_DEVICE(vendor, device)
 *   - vendor = 0x1af4  (Red Hat/QEMU 的 PCI vendor ID)
 *   - device = 0x1110  (ivshmem-plain 设备 ID)
 *
 * 最后一个 {0,} 是结束标记，必须有
 *
 * MODULE_DEVICE_TABLE 的作用：
 *   让内核在模块加载时知道把这个 id 列表导出到 sysfs，
 *   这样 PCI bus 在枚举时会匹配到这个驱动
 */
static const struct pci_device_id day22_pci_ids[] = {
    { PCI_DEVICE(DAY22_IVSHMEM_VENDOR_ID, DAY22_IVSHMEM_DEVICE_ID) },
    { 0, }  /* 结束标记 */
};
MODULE_DEVICE_TABLE(pci, day22_pci_ids);

/*
 * ==================== 第3步：读取并打印 BAR 资源信息 ====================
 *
 * 这个函数演示了如何读取 PCI 配置空间中的 BAR（Base Address Registers）
 *
 * PCI 设备通过 BAR 告诉系统自己使用了哪些内存/IO 地址：
 *   BAR0: 我使用 N 字节的 MMIO，起始地址由系统分配后写入
 *   BAR1: 我使用 M 字节的 MMIO
 *   ...
 *
 * 注意：这里只是"读取"配置空间中的值，还没做 iomap 映射
 *       映射是 day23 的 pci_iomap() 做的事情
 *
 * pci_resource_* 是 Linux 内核标准 API：
 *   - pci_resource_start()  获取 BAR 起始地址
 *   - pci_resource_end()    获取 BAR 结束地址
 *   - pci_resource_len()    获取 BAR 长度
 *   - pci_resource_flags()  获取 BAR 属性（Memory/IO 等）
 */
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

/*
 * ==================== 第4步：probe - 设备插入时调用 ====================
 *
 * 当内核的 PCI core 枚举到一个设备，且设备的 vendor:device 匹配
 * pdev->vendor == 0x1af4 && pdev->device == 0x1110
 * 就会自动调用这个函数
 *
 * 参数：
 *   pdev  - 指向枚举到的 PCI 设备（struct pci_dev）
 *   id    - 指向匹配的 pci_device_id 条目
 *
 * 返回值：
 *   0      = 成功，内核保留这个驱动
 *   负数   = 失败，内核会卸载这个驱动
 *
 * ==================== probe 内部分解 ====================
 *
 *  ① devm_kzalloc() 分配私有数据结构
 *       devm_* 是"devres"版本，失败时自动释放，无需手动 free
 *       比普通的 kzalloc 更安全
 *
 *  ② pci_set_drvdata(pdev, sdev)
 *       把私有数据 sdev 存到 pdev 里
 *       这样之后在 remove / 其他函数里，
 *       通过 pci_get_drvdata(pdev) 就能取出 sdev
 *
 *  ③ day22_dump_resources(pdev)
 *       读取并打印 BAR 信息（从 PCI config space 读，还没映射）
 *
 *  ④ 注意：今天故意不做以下操作（留到 day23/day25）
 *       - pci_enable_device()      使能 PCI 设备
 *       - pci_request_regions()    请求 BAR 资源
 *       - pci_iomap()              映射 BAR 到虚拟地址
 *       - pci_alloc_irq_vectors()  分配 MSI 中断向量
 *       - request_irq()            注册中断处理函数
 */
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

/*
 * ==================== 第5步：remove - 设备拔出时调用 ====================
 *
 * 当 PCI 设备被拔出，或者驱动被卸载（rmmod）时调用
 *
 * 重点：
 *   - 由于使用的是 devm_kzalloc()，内核会自动释放内存
 *   - 不需要手动 kfree
 *   - 如果 day23 做了 pci_request_regions()，这里需要 pci_release_regions()
 *   - 如果 day23 做了 pci_iomap()，这里需要 pci_iounmap()
 *   - 如果 day25 做了 request_irq()，这里需要 free_irq()
 */
static void day22_remove(struct pci_dev *pdev)
{
    struct day22_stub_dev *sdev = pci_get_drvdata(pdev);

    dev_info(&pdev->dev,
             "remove enter: sdev=%p irq_vector=%d irq_count=%llu\n",
             sdev, sdev ? sdev->irq_vector : -1,
             sdev ? sdev->irq_count : 0ULL);
}

/*
 * ==================== 第6步：pci_driver 结构体 ====================
 *
 * 这是驱动注册到内核的核心结构体
 *
 * 重要字段：
 *   .name      - 驱动名称，用于日志显示
 *   .id_table  - 指向 pci_device_id 数组，内核靠这个匹配设备
 *   .probe     - 设备插入回调函数指针
 *   .remove    - 设备拔出回调函数指针
 *
 * 注意：内核用 id_table 做匹配，所以 .probe / .remove 不需要
 *       再自己判断 vendor/device，匹配上了就一定是你声明的设备
 */
static struct pci_driver day22_pci_driver = {
    .name = DRV_NAME,
    .id_table = day22_pci_ids,
    .probe = day22_probe,
    .remove = day22_remove,
};

/*
 * ==================== 第7步：模块入口 - 简化写法 ====================
 *
 * module_pci_driver() 是一个宏，等价于：
 *
 *   static int __init day22_init(void)
 *   {
 *       return pci_register_driver(&day22_pci_driver);
 *   }
 *   module_init(day22_init);
 *
 *   static void __exit day22_exit(void)
 *   {
 *       pci_unregister_driver(&day22_pci_driver);
 *   }
 *   module_exit(day22_exit);
 *
 * 为什么用这个简化写法？
 *   因为 pci_driver 的注册/注销顺序是固定的，
 *   不像字符设备需要考虑 major/minor 号管理
 */
module_pci_driver(day22_pci_driver);

/*
 * ==================== 模块信息 ====================
 */
MODULE_AUTHOR("OpenAI / WangQi day22 lab scaffold");
MODULE_DESCRIPTION("day22 ivshmem pci stub for learning probe/remove lifecycle");
MODULE_LICENSE("GPL");

/*
 * ==================== 附录：完整的 pci_driver 学习路径 ====================
 *
 *  ① day22（今天）
 *     理解 pci_driver 骨架：id_table / probe / remove
 *     理解 BAR 信息的读取（pci_resource_*）
 *     理解 devm_kzalloc / pci_set_drvdata / pci_get_drvdata
 *
 *  ② day23
 *     pci_enable_device()        - 使能 PCI 设备
 *     pci_request_regions()      - 请求 BAR 资源独占访问
 *     pci_iomap()                - 将 BAR 映射到内核虚拟地址
 *     pci_resource_start/len()   - 获取 BAR 地址和长度
 *
 *  ③ day25
 *     pci_alloc_irq_vectors()    - 分配 MSI 中断向量
 *     request_irq()              - 注册中断处理函数（顶半部）
 *
 *  ④ probe 的对称性原则
 *     probe 中申请的所有资源，必须在 remove 中按逆序释放：
 *
 *     probe 顺序：                          remove 顺序：
 *     1. devm_kzalloc (私有数据)           1. (devm 自动释放)
 *     2. pci_enable_device                 2. pci_disable_device
 *     3. pci_request_regions               3. pci_release_regions
 *     4. pci_iomap (BAR 映射)              4. pci_iounmap
 *     5. pci_alloc_irq_vectors             5. pci_free_irq_vectors
 *     6. request_irq (中断处理)             6. free_irq
 */
